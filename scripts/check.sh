#!/usr/bin/env bash
#
# Configure, build, and run the full ctest suite. CI runs this same script on
# every pull request (.github/workflows/ci.yml), so a local pass predicts CI.
#
# Usage: scripts/check.sh [--build-dir <path>]
#
#   --build-dir P   Build directory (default: build, so repeat runs are
#                   incremental and reuse the fetched dependencies).
#
# audio_test and music_clock_test open the default sound device. When the host
# has one, ctest runs inside a bwrap sandbox that hides it, so the suite stays
# silent. Set BLAZE_CHECK_LIVE_AUDIO=1 to run against the real device.
#
set -euo pipefail

usage() {
    cat <<'USAGE'
Usage: scripts/check.sh [--build-dir <path>]

  --build-dir P   Build directory (default: build).
  -h, --help      Show this help.

Environment:
  BLAZE_CHECK_LIVE_AUDIO=1   Let the tests open the real sound device.
USAGE
}

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="$repo_root/build"
while [ $# -gt 0 ]; do
    case "$1" in
        --build-dir)
            build_dir="${2:?--build-dir requires a path}"
            shift 2
            ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage; exit 2 ;;
    esac
done

jobs="$(nproc 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"
log_dir="$build_dir/check-logs"
mkdir -p "$log_dir"

# Run one step with its output in a log file, so a passing run prints a few
# lines. On failure, print the log: all of it in CI (the file is gone after the
# run), and its last 80 lines locally.
run_step() {
    local name="$1" log="$log_dir/$1.log"
    shift
    if "$@" >"$log" 2>&1; then
        return
    fi
    echo ""
    echo "CHECK: FAIL"
    echo "  step: $name"
    echo "  log:  $log"
    if [ -n "${CI:-}" ]; then
        cat "$log"
    else
        echo "  ----- last 80 log lines -----"
        tail -n 80 "$log"
    fi
    exit 1
}

echo "[1/3] Configure..."
run_step configure cmake -S "$repo_root" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

echo "[2/3] Build (-j$jobs)..."
run_step build cmake --build "$build_dir" -j"$jobs"

echo "[3/3] Test..."
ctest_cmd=(ctest --test-dir "$build_dir" --output-on-failure)
if [ -d /dev/snd ] && [ "${BLAZE_CHECK_LIVE_AUDIO:-0}" != "1" ]; then
    if ! command -v bwrap >/dev/null 2>&1; then
        echo "check.sh: this host has a sound device and bwrap is not installed." >&2
        echo "Install bubblewrap, or set BLAZE_CHECK_LIVE_AUDIO=1 to use the real device." >&2
        exit 1
    fi
    echo "Sound device found: running the tests in a sandbox that hides it."
    ctest_cmd=(bwrap --dev-bind / / --tmpfs "/run/user/$(id -u)" --tmpfs /dev/snd
        --unshare-net "${ctest_cmd[@]}")
fi
run_step ctest "${ctest_cmd[@]}"

echo ""
echo "CHECK: PASS"
echo "  $(grep -E '[0-9]+% tests passed' "$log_dir/ctest.log" | tail -n 1)"
