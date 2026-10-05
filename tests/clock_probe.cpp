// clock_probe (#81): music-clock granularity / interpolation probe through the
// real AudioEngine + SoundStream path. The committed successor of the scratch
// `gran2` probe from #71 (docs/AUDIO_LATENCY.md, "Reproducing the probes").
// Not a ctest: it measures a device, so results depend on the host.
//
//   clock_probe [period_frames=480] [--null] [--seconds 3]
//
// SILENT: it plays an all-zero WAV at volume 0. It never mutes, reroutes or
// changes the default device. A small requested period briefly lowers the
// PipeWire graph quantum for every app while it runs (a few seconds), so other
// apps may glitch meanwhile.
//
// To measure a real device, run it OUTSIDE the bwrap test sandbox (the sandbox
// has no audio device; without --null the engine then fails to open one).
// `--null` uses miniaudio's silent, real-time-paced null backend instead
// (sandbox-safe; residuals are expected to be near zero).
//
// Method (same as gran2): poll every 1 ms, recording (now_ns, raw cursor,
// interpolated cursor). Fit a least-squares line to the raw cursor over the
// run (the ideal steady clock), skip the first and last 0.3 s, remove the mean
// bias (calibration absorbs it), and report rms / max residual of the raw and
// the interpolated cursor.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "audio/audio_engine.hpp"
#include "audio/sound_stream.hpp"
#include "test_wav_writer.hpp"

namespace fs = std::filesystem;

namespace {

uint64_t steady_ns() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                     std::chrono::steady_clock::now().time_since_epoch())
                                     .count());
}

struct Poll {
    uint64_t ns = 0;
    uint64_t raw = 0;
    uint64_t interp = 0;
};

struct Residual {
    double rms_ms = 0.0;
    double max_ms = 0.0;
};

// Residuals of `value(i)` against the line y = a + b * t (t in s since t0), bias removed.
template <typename Value>
Residual residual(const std::vector<Poll>& polls, std::size_t lo, std::size_t hi, double a, double b,
                  uint64_t t0, double rate, Value value) {
    std::vector<double> r;
    r.reserve(hi - lo);
    for (std::size_t i = lo; i < hi; ++i) {
        const double t = static_cast<double>(polls[i].ns - t0) / 1e9;
        r.push_back((static_cast<double>(value(polls[i])) - (a + b * t)) / rate * 1000.0);
    }
    double mean = 0.0;
    for (double x : r) mean += x;
    mean /= static_cast<double>(r.size());
    Residual out;
    for (double x : r) {
        const double d = x - mean;
        out.rms_ms += d * d;
        out.max_ms = std::max(out.max_ms, std::abs(d));
    }
    out.rms_ms = std::sqrt(out.rms_ms / static_cast<double>(r.size()));
    return out;
}

void usage() {
    std::cerr << "usage: clock_probe [period_frames=480] [--null] [--seconds 3]\n";
}

} // namespace

int main(int argc, char** argv) {
    uint32_t period = blaze4k::kDefaultAudioPeriodFrames;
    bool use_null = false;
    double seconds = 3.0;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--null") {
            use_null = true;
        } else if (arg == "--seconds" && i + 1 < argc) {
            seconds = std::atof(argv[++i]);
        } else if (arg == "-h" || arg == "--help") {
            usage();
            return 0;
        } else if (!arg.empty() && arg.find_first_not_of("0123456789") == std::string::npos) {
            period = static_cast<uint32_t>(std::strtoul(arg.c_str(), nullptr, 10));
        } else {
            usage();
            return 2;
        }
    }
    if (!(seconds >= 1.0 && seconds <= 4.0)) {
        std::cerr << "[clock_probe] --seconds must be in [1, 4] (the silent clip is 5 s)\n";
        return 2;
    }

    blaze4k::AudioEngine& engine = blaze4k::AudioEngine::instance();
    engine.configure({period, +[]() -> uint64_t { return steady_ns(); }, use_null});
    if (!engine.init()) {
        std::cerr << "[clock_probe] could not open an audio device"
                  << (use_null ? " (null backend)" : "") << "\n";
        return 1;
    }
    const uint32_t rate = engine.engine_sample_rate();

    const fs::path wav = fs::temp_directory_path() / "blaze4k_clock_probe_silence.wav";
    if (!write_silent_wav(wav.string(), 5.0, rate, 2)) {
        std::cerr << "[clock_probe] could not write " << wav << "\n";
        return 1;
    }

    int status = 0;
    {
        blaze4k::SoundStream stream;
        if (!stream.load(wav.string())) {
            std::cerr << "[clock_probe] could not load the silent clip\n";
            fs::remove(wav);
            return 1;
        }
        stream.set_volume(0.0f);
        const bool interpolating = stream.enable_clock_interpolation();
        if (!interpolating) {
            std::cerr << "[clock_probe] clock interpolation unavailable\n";
            status = 1;
        }
        stream.play();

        std::vector<Poll> polls;
        polls.reserve(static_cast<std::size_t>(seconds * 1200));
        const uint64_t start = steady_ns();
        const auto duration_ns = static_cast<uint64_t>(seconds * 1e9);
        while (steady_ns() - start < duration_ns) {
            const blaze4k::TimedFrames timed = stream.get_timed_position_frames();
            const uint64_t raw = stream.get_raw_position_frames();
            polls.push_back(Poll{timed.timestamp_ns != 0 ? timed.timestamp_ns : steady_ns(), raw,
                                 timed.frames});
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        stream.stop();
        engine.log_callback_stats("clock_probe");
        stream.unload();

        // Raw step statistics (distinct cursor changes).
        uint64_t step_min = UINT64_MAX;
        uint64_t step_max = 0;
        uint64_t step_sum = 0;
        uint64_t steps = 0;
        for (std::size_t i = 1; i < polls.size(); ++i) {
            if (polls[i].raw > polls[i - 1].raw) {
                const uint64_t step = polls[i].raw - polls[i - 1].raw;
                step_min = std::min(step_min, step);
                step_max = std::max(step_max, step);
                step_sum += step;
                ++steps;
            }
        }

        // Fit window: skip the first and last 0.3 s.
        const uint64_t t0 = polls.empty() ? 0 : polls.front().ns;
        const uint64_t t_end = polls.empty() ? 0 : polls.back().ns;
        std::size_t lo = 0;
        std::size_t hi = polls.size();
        while (lo < polls.size() && polls[lo].ns < t0 + 300'000'000ULL) ++lo;
        while (hi > lo && polls[hi - 1].ns > t_end - 300'000'000ULL) --hi;
        if (steps == 0 || hi <= lo + 10 || rate == 0) {
            std::cerr << "[clock_probe] cursor did not advance; nothing to measure\n";
            fs::remove(wav);
            return 1;
        }

        double st = 0, sy = 0, stt = 0, sty = 0;
        const auto n = static_cast<double>(hi - lo);
        for (std::size_t i = lo; i < hi; ++i) {
            const double t = static_cast<double>(polls[i].ns - t0) / 1e9;
            const auto y = static_cast<double>(polls[i].raw);
            st += t;
            sy += y;
            stt += t * t;
            sty += t * y;
        }
        const double b = (n * sty - st * sy) / (n * stt - st * st);
        const double a = (sy - b * st) / n;

        const Residual raw_r =
            residual(polls, lo, hi, a, b, t0, rate, [](const Poll& p) { return p.raw; });
        const Residual interp_r =
            residual(polls, lo, hi, a, b, t0, rate, [](const Poll& p) { return p.interp; });
        double lead = 0.0;
        for (std::size_t i = lo; i < hi; ++i) {
            lead += static_cast<double>(polls[i].interp) - static_cast<double>(polls[i].raw);
        }
        lead = lead / n / rate * 1000.0;
        const blaze4k::AudioEngine::CallbackStats cb = engine.callback_stats();

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "[clock_probe] requested period " << period << " frames"
                  << (use_null ? " (null backend)" : "") << ", engine rate " << rate << " Hz, "
                  << polls.size() << " polls over " << seconds << " s, fitted rate " << b
                  << " frames/s\n";
        std::cout << "[clock_probe] raw step min / mean / max: " << step_min << " / "
                  << (static_cast<double>(step_sum) / static_cast<double>(steps)) << " / "
                  << step_max << " frames (" << steps << " steps)\n";
        std::cout << "[clock_probe] engine callback interval: min " << cb.min_frames << " / max "
                  << cb.max_frames << " frames over " << cb.callbacks << " callbacks\n";
        std::cout << "[clock_probe] raw residual rms " << raw_r.rms_ms << " ms / max "
                  << raw_r.max_ms << " ms\n";
        std::cout << "[clock_probe] interpolated residual rms " << interp_r.rms_ms << " ms / max "
                  << interp_r.max_ms << " ms\n";
        std::cout << "[clock_probe] interpolated mean lead over raw: " << lead << " ms\n";
    }

    engine.shutdown();
    std::error_code ec;
    fs::remove(wav, ec);
    return status;
}
