#include <array>
#include <iostream>
#include <cmath>
#include <string>
#include <vector>
#include <limits>
#include "chart/chart.hpp"
#include "chart/timing_data.hpp"
#include "gameplay/speed_mod.hpp"
#include "gameplay/note_field.hpp"
#include "gameplay/noteskin.hpp"

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " #expr << "\n"; \
            std::abort(); \
        } \
    } while (0)

namespace {

bool approx(double a, double b, double epsilon = 1e-9) {
    return std::abs(a - b) < epsilon;
}

blaze4k::Note make_note(int column, double beat, double time_seconds, blaze4k::NoteType type,
                   double hold_beats = 0.0, double hold_end_time = 0.0) {
    blaze4k::Note note;
    note.column = column;
    note.beat = beat;
    note.time_seconds = time_seconds;
    note.type = type;
    note.hold_length_beats = hold_beats;
    note.hold_end_time_seconds = hold_end_time;
    return note;
}

} // namespace

int main() {
    std::cout << "[note_field_test] Starting note field tests...\n";

    // 1. Speed-mod parsing (OpenITG semantics, case-insensitive, positive floats only).
    {
        blaze4k::SpeedMod mod;

        TEST_CHECK(blaze4k::parse_speed_mod("C400", mod));
        TEST_CHECK(mod.type == blaze4k::SpeedModType::CMod && approx(mod.value, 400.0));

        TEST_CHECK(blaze4k::parse_speed_mod("1.5x", mod));
        TEST_CHECK(mod.type == blaze4k::SpeedModType::XMod && approx(mod.value, 1.5));

        TEST_CHECK(blaze4k::parse_speed_mod("M600", mod));
        TEST_CHECK(mod.type == blaze4k::SpeedModType::MMod && approx(mod.value, 600.0));

        TEST_CHECK(blaze4k::parse_speed_mod("c400", mod));
        TEST_CHECK(mod.type == blaze4k::SpeedModType::CMod && approx(mod.value, 400.0));

        TEST_CHECK(blaze4k::parse_speed_mod("X2", mod));
        TEST_CHECK(mod.type == blaze4k::SpeedModType::XMod && approx(mod.value, 2.0));

        TEST_CHECK(!blaze4k::parse_speed_mod("", mod));
        TEST_CHECK(!blaze4k::parse_speed_mod("abc", mod));
        TEST_CHECK(!blaze4k::parse_speed_mod("0x", mod));
        TEST_CHECK(!blaze4k::parse_speed_mod("-1x", mod));
        TEST_CHECK(!blaze4k::parse_speed_mod("C0", mod));
        TEST_CHECK(!blaze4k::parse_speed_mod("infx", mod));
        std::cout << "  - Speed-mod parsing correct.\n";
    }

    // 2. C-mod is constant across a BPM change (time spacing).
    {
        blaze4k::TimingData timing_a;
        timing_a.parse_bpms_string("0=120");
        blaze4k::TimingData timing_b;
        timing_b.parse_bpms_string("0=120,8=240");

        blaze4k::Chart chart_a;
        chart_a.timing = timing_a;
        chart_a.notes.push_back(make_note(0, 4.0, 2.0, blaze4k::NoteType::Tap));

        blaze4k::Chart chart_b;
        chart_b.timing = timing_b;
        chart_b.notes.push_back(make_note(0, 4.0, 2.0, blaze4k::NoteType::Tap));

        blaze4k::NoteField field_a;
        field_a.set_chart(&chart_a);
        field_a.set_speed_mod(blaze4k::SpeedMod{blaze4k::SpeedModType::CMod, 400.0});
        blaze4k::NoteField field_b;
        field_b.set_chart(&chart_b);
        field_b.set_speed_mod(blaze4k::SpeedMod{blaze4k::SpeedModType::CMod, 400.0});

        const double expected = (2.0 - 1.0) * (400.0 / 60.0) * 64.0;
        const double offset_a = field_a.offset_for_note(chart_a.notes.front(), 1.0);
        const double offset_b = field_b.offset_for_note(chart_b.notes.front(), 1.0);
        TEST_CHECK(approx(offset_a, expected));
        TEST_CHECK(approx(offset_a, offset_b));
        std::cout << "  - C-mod offset is independent of BPM layout.\n";
    }

    // 3. X-mod beat spacing is invariant across a BPM change.
    {
        blaze4k::TimingData timing;
        timing.parse_bpms_string("0=120,8=240");

        blaze4k::Chart chart;
        chart.timing = timing;
        chart.notes.push_back(make_note(0, 16.0, 0.0, blaze4k::NoteType::Tap));

        blaze4k::NoteField field;
        field.set_chart(&chart);
        field.set_speed_mod(blaze4k::SpeedMod{blaze4k::SpeedModType::XMod, 1.0});

        const double t_low = timing.beat_to_seconds(8.0);   // inside first BPM
        const double t_high = timing.beat_to_seconds(12.0); // inside second BPM

        const double offset_low = field.offset_for_note(chart.notes.front(), t_low);
        const double offset_high = field.offset_for_note(chart.notes.front(), t_high);

        TEST_CHECK(approx(offset_low, (16.0 - 8.0) * 64.0));
        TEST_CHECK(approx(offset_high, (16.0 - 12.0) * 64.0));

        const double beats_low = offset_low / 64.0;
        const double beats_high = offset_high / 64.0;
        TEST_CHECK(approx(beats_low, 8.0));
        TEST_CHECK(approx(beats_high, 4.0));
        std::cout << "  - X-mod beat spacing follows BPM segments.\n";
    }

    // 3b. X-mod 8x (#61) scales beat spacing exactly 8x relative to 1x.
    {
        blaze4k::TimingData timing;
        timing.parse_bpms_string("0=120");

        blaze4k::Chart chart;
        chart.timing = timing;
        chart.notes.push_back(make_note(0, 16.0, timing.beat_to_seconds(16.0), blaze4k::NoteType::Tap));

        blaze4k::NoteField x1;
        x1.set_chart(&chart);
        x1.set_speed_mod(blaze4k::SpeedMod{blaze4k::SpeedModType::XMod, 1.0});

        blaze4k::NoteField x8;
        x8.set_chart(&chart);
        x8.set_speed_mod(blaze4k::SpeedMod{blaze4k::SpeedModType::XMod, 8.0});

        TEST_CHECK(approx(x8.effective_x_speed(), 8.0));
        const blaze4k::Note& note = chart.notes.front();
        for (double t = 0.0; t < 8.0; t += 0.5) {
            TEST_CHECK(approx(x8.offset_for_note(note, t), 8.0 * x1.offset_for_note(note, t), 1e-6));
        }
        TEST_CHECK(approx(x1.offset_for_note(note, 0.0), 16.0 * 64.0));
        TEST_CHECK(approx(x8.offset_for_note(note, 0.0), 8.0 * 16.0 * 64.0, 1e-6));
        std::cout << "  - X-mod 8x scales beat spacing 8x.\n";
    }

    // 4. M-mod resolves to an X-mod via the chart's max BPM.
    {
        blaze4k::TimingData timing;
        timing.parse_bpms_string("0=100,16=150");

        blaze4k::Chart chart;
        chart.timing = timing;
        chart.notes.push_back(make_note(0, 16.0, 0.0, blaze4k::NoteType::Tap));

        TEST_CHECK(approx(blaze4k::max_chart_bpm(timing), 150.0));
        TEST_CHECK(approx(blaze4k::resolve_x_speed(blaze4k::SpeedMod{blaze4k::SpeedModType::MMod, 600.0}, timing), 4.0));
        TEST_CHECK(approx(blaze4k::resolve_x_speed(blaze4k::SpeedMod{blaze4k::SpeedModType::XMod, 3.0}, timing), 3.0));

        blaze4k::NoteField m_field;
        m_field.set_chart(&chart);
        m_field.set_speed_mod(blaze4k::SpeedMod{blaze4k::SpeedModType::MMod, 600.0});

        blaze4k::NoteField x_field;
        x_field.set_chart(&chart);
        x_field.set_speed_mod(blaze4k::SpeedMod{blaze4k::SpeedModType::XMod, 4.0});

        TEST_CHECK(approx(m_field.effective_x_speed(), 4.0));
        for (double t = 0.0; t < 4.0; t += 0.25) {
            TEST_CHECK(approx(m_field.offset_for_note(chart.notes.front(), t),
                              x_field.offset_for_note(chart.notes.front(), t)));
        }
        std::cout << "  - M-mod equals the equivalent X-mod.\n";
    }

    // 5. Offset approaches zero then passes as only the supplied music time advances.
    {
        blaze4k::TimingData timing;
        timing.parse_bpms_string("0=120");

        blaze4k::Chart chart;
        chart.timing = timing;
        chart.notes.push_back(make_note(0, 8.0, 4.0, blaze4k::NoteType::Tap));

        blaze4k::NoteField field;
        field.set_chart(&chart);
        field.set_speed_mod(blaze4k::SpeedMod{blaze4k::SpeedModType::XMod, 1.0});

        const double t_note = timing.beat_to_seconds(8.0);
        const double before = field.offset_for_note(chart.notes.front(), t_note - 1.0);
        const double at = field.offset_for_note(chart.notes.front(), t_note);
        const double after = field.offset_for_note(chart.notes.front(), t_note + 1.0);

        TEST_CHECK(before > 0.0);
        TEST_CHECK(after < 0.0);
        TEST_CHECK(before > at);
        TEST_CHECK(at > after);
        TEST_CHECK(approx(at, 0.0));
        std::cout << "  - Offset is driven purely by the supplied music time.\n";
    }

    // 6. Scroll direction is a pure mirror about the receptor row.
    {
        blaze4k::NoteField field;
        blaze4k::NoteFieldConfig config;
        config.receptor_y = 100.0;

        config.direction = blaze4k::ScrollDirection::Up;
        field.set_config(config);
        const double up = field.screen_y(40.0);

        config.direction = blaze4k::ScrollDirection::Down;
        field.set_config(config);
        const double down = field.screen_y(40.0);

        TEST_CHECK(approx(up, 140.0));
        TEST_CHECK(approx(down, 60.0));
        TEST_CHECK(approx((up + down) * 0.5, 100.0));
        std::cout << "  - Up/down scroll mirror about the receptor.\n";
    }

    // 7. Stop semantics: X-mod freezes during a stop; C-mod keeps moving.
    {
        blaze4k::TimingData timing;
        timing.parse_bpms_string("0=120");
        timing.add_stop(4.0, 1.0);

        blaze4k::Chart chart;
        chart.timing = timing;
        chart.notes.push_back(make_note(0, 8.0, timing.beat_to_seconds(8.0), blaze4k::NoteType::Tap));

        blaze4k::NoteField x_field;
        x_field.set_chart(&chart);
        x_field.set_speed_mod(blaze4k::SpeedMod{blaze4k::SpeedModType::XMod, 1.0});

        const double stop_start = timing.beat_to_seconds(4.0);
        const double x_early = x_field.offset_for_note(chart.notes.front(), stop_start + 0.2);
        const double x_late = x_field.offset_for_note(chart.notes.front(), stop_start + 0.8);
        TEST_CHECK(approx(x_early, x_late));

        blaze4k::NoteField c_field;
        c_field.set_chart(&chart);
        c_field.set_speed_mod(blaze4k::SpeedMod{blaze4k::SpeedModType::CMod, 400.0});

        const double c_early = c_field.offset_for_note(chart.notes.front(), stop_start + 0.2);
        const double c_late = c_field.offset_for_note(chart.notes.front(), stop_start + 0.8);
        TEST_CHECK(c_early > c_late);
        std::cout << "  - X-mod freezes through a stop; C-mod does not.\n";
    }

    // 8. Holds/rolls/mines render distinctly.
    {
        blaze4k::TimingData timing;
        timing.parse_bpms_string("0=120");

        blaze4k::Chart chart;
        chart.timing = timing;
        chart.notes.push_back(make_note(0, 4.0, timing.beat_to_seconds(4.0), blaze4k::NoteType::Tap));
        chart.notes.push_back(make_note(1, 4.0, timing.beat_to_seconds(4.0), blaze4k::NoteType::HoldHead,
                                        4.0, timing.beat_to_seconds(8.0)));
        chart.notes.push_back(make_note(2, 8.0, timing.beat_to_seconds(8.0), blaze4k::NoteType::RollHead,
                                        4.0, timing.beat_to_seconds(12.0)));
        chart.notes.push_back(make_note(3, 4.0, timing.beat_to_seconds(4.0), blaze4k::NoteType::Mine));

        blaze4k::NoteField field;
        field.set_chart(&chart);
        field.set_speed_mod(blaze4k::SpeedMod{blaze4k::SpeedModType::XMod, 1.0});

        std::vector<blaze4k::NoteRenderItem> items;
        field.compute_visible(0.0, -1000.0, 2000.0, items);
        TEST_CHECK(items.size() == 4);

        const blaze4k::NoteRenderItem* tap = nullptr;
        const blaze4k::NoteRenderItem* hold = nullptr;
        const blaze4k::NoteRenderItem* roll = nullptr;
        const blaze4k::NoteRenderItem* mine = nullptr;
        for (const blaze4k::NoteRenderItem& item : items) {
            switch (item.type) {
                case blaze4k::NoteType::Tap: tap = &item; break;
                case blaze4k::NoteType::HoldHead: hold = &item; break;
                case blaze4k::NoteType::RollHead: roll = &item; break;
                case blaze4k::NoteType::Mine: mine = &item; break;
            }
        }

        TEST_CHECK(tap != nullptr && !tap->has_body);
        TEST_CHECK(hold != nullptr && hold->has_body && hold->tail_offset > hold->head_offset);
        TEST_CHECK(roll != nullptr && roll->has_body && roll->tail_offset > roll->head_offset);
        TEST_CHECK(mine != nullptr && !mine->has_body);

        // Headless (no init): the procedural fallback skin's sprites.
        blaze4k::NoteSkin skin;
        const std::array<blaze4k::SkinSprite, 2> mine_layers = skin.mine(0.0);
        TEST_CHECK(mine_layers[0].scale < 1.0f);
        // ITG note colors encode the beat subdivision (not column direction):
        // 4th=red, 8th=blue, 16th=yellow.
        const blaze4k::Color fourth = skin.quantization_color(blaze4k::NoteQuantization::Fourth);
        const blaze4k::Color eighth = skin.quantization_color(blaze4k::NoteQuantization::Eighth);
        const blaze4k::Color sixteenth = skin.quantization_color(blaze4k::NoteQuantization::Sixteenth);
        TEST_CHECK(fourth.r > fourth.g && fourth.r > fourth.b);
        TEST_CHECK(eighth.b > eighth.r && eighth.b > eighth.g);
        TEST_CHECK(sixteenth.r > sixteenth.b && sixteenth.g > sixteenth.b);
        TEST_CHECK(mine_layers[0].tint.r > mine_layers[0].tint.g);
        std::cout << "  - Tap/hold/roll/mine are visually distinct.\n";
    }

    // 8b. C-mod tail and beat offsets use absolute time spacing (not beat spacing).
    {
        blaze4k::TimingData timing;
        timing.parse_bpms_string("0=120,8=240");

        const double head_time = timing.beat_to_seconds(4.0);
        const double tail_time = timing.beat_to_seconds(8.0);

        blaze4k::Chart chart;
        chart.timing = timing;
        chart.notes.push_back(make_note(0, 4.0, head_time, blaze4k::NoteType::HoldHead,
                                        4.0, tail_time));

        blaze4k::NoteField field;
        field.set_chart(&chart);
        field.set_speed_mod(blaze4k::SpeedMod{blaze4k::SpeedModType::CMod, 400.0});

        const double spacing = (400.0 / 60.0) * 64.0;
        const double head = field.offset_for_note(chart.notes.front(), 1.0);
        const double tail = field.tail_offset_for(chart.notes.front(), 1.0);
        TEST_CHECK(approx(head, (head_time - 1.0) * spacing));
        TEST_CHECK(approx(tail, (tail_time - 1.0) * spacing));
        TEST_CHECK(tail > head);

        // offset_for_beat's C-mod branch maps the beat to seconds first.
        TEST_CHECK(approx(field.offset_for_beat(8.0, 1.0), (tail_time - 1.0) * spacing));
        std::cout << "  - C-mod tail and beat offsets use absolute time spacing.\n";
    }

    // 9. Culling keeps bodies that intersect the visible window.
    {
        blaze4k::TimingData timing;
        timing.parse_bpms_string("0=120");

        blaze4k::Chart chart;
        chart.timing = timing;
        chart.notes.push_back(make_note(0, 2.0, timing.beat_to_seconds(2.0), blaze4k::NoteType::Tap));
        chart.notes.push_back(make_note(1, 4.0, timing.beat_to_seconds(4.0), blaze4k::NoteType::HoldHead,
                                        4.0, timing.beat_to_seconds(8.0)));

        blaze4k::NoteField field;
        field.set_chart(&chart);
        field.set_speed_mod(blaze4k::SpeedMod{blaze4k::SpeedModType::XMod, 1.0});

        std::vector<blaze4k::NoteRenderItem> items;

        // Tap center at offset 128: padding by half a note (28px) keeps it when
        // the window edge is at 100, but drops it once the edge plus padding is
        // still short of the center.
        field.compute_visible(0.0, 0.0, 100.0, items);
        TEST_CHECK(items.size() == 1);
        TEST_CHECK(items.front().type == blaze4k::NoteType::Tap);

        field.compute_visible(0.0, 0.0, 90.0, items);
        TEST_CHECK(items.empty());

        field.compute_visible(0.0, 100.0, 200.0, items);
        TEST_CHECK(items.size() == 1);
        TEST_CHECK(items.front().type == blaze4k::NoteType::Tap);

        // Hold body [256, 512] intersects [300, 400] even with its head off-screen.
        field.compute_visible(0.0, 300.0, 400.0, items);
        TEST_CHECK(items.size() == 1);
        TEST_CHECK(items.front().type == blaze4k::NoteType::HoldHead);

        field.compute_visible(0.0, 600.0, 700.0, items);
        TEST_CHECK(items.empty());
        std::cout << "  - Culling includes partially visible holds.\n";
    }

    // 10. Column layout is strictly increasing in L,D,U,R order.
    {
        blaze4k::NoteField field;
        blaze4k::NoteFieldConfig config;
        config.column_width = 64.0;
        field.set_config(config);

        const double x0 = field.column_x(0, 0.0);
        const double x1 = field.column_x(1, 0.0);
        const double x2 = field.column_x(2, 0.0);
        const double x3 = field.column_x(3, 0.0);

        TEST_CHECK(x0 < x1 && x1 < x2 && x2 < x3);
        TEST_CHECK(approx(x0, 32.0) && approx(x3, 224.0));
        TEST_CHECK(approx(field.field_width(), 256.0));
        std::cout << "  - Column layout is ordered and centered.\n";
    }

    // 11. Shared field_left: the centred field's left edge (HUD + field renderer).
    {
        blaze4k::NoteField field;
        blaze4k::NoteFieldConfig config;
        config.column_width = 108.0;
        field.set_config(config);

        TEST_CHECK(approx(field.field_left(1280), 424.0));
        TEST_CHECK(approx(field.field_left(320), -56.0));
        for (const int w : {320, 640, 1024, 1280, 1920}) {
            TEST_CHECK(approx(field.field_left(w) + field.field_width() * 0.5, w * 0.5));
        }
        std::cout << "  - field_left centres the field on the screen.\n";
    }

    std::cout << "[note_field_test] All note field tests passed successfully!\n";
    return 0;
}
