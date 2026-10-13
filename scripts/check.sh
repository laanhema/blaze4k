#!/usr/bin/env bash
#
# Lint, configure, build with warnings as errors, and run the full ctest suite.
# CI runs this same script on every pull request (.github/workflows/ci.yml), so
# a local pass predicts CI.
#
# A warning in a project source fails the build step, so it is reported again
# on every run until it is fixed: there is no need to recompile changed files
# by hand to look for warnings. Dependency warnings do not count.
#
# Usage: scripts/check.sh [--build-dir <path>]
#
#   --build-dir P   Build directory (default: build, so repeat runs are
#                   incremental and reuse the fetched dependencies).
#
# The tests are silent: the audio engine opens miniaudio's null device unless
# it is asked for the real one, and scripts/lint.sh allows only the game and
# tests/clock_probe.cpp to ask.
#
set -euo pipefail

usage() {
    cat <<'USAGE'
Usage: scripts/check.sh [--build-dir <path>]

  --build-dir P   Build directory (default: build).
  -h, --help      Show this help.
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

echo "[1/4] Lint..."
run_step lint "$repo_root/scripts/lint.sh"

echo "[2/4] Configure..."
run_step configure cmake -S "$repo_root" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release \
    -DBLAZE_WERROR=ON

echo "[3/4] Build (-j$jobs)..."
run_step build cmake --build "$build_dir" -j"$jobs"

echo "[4/4] Test..."
run_step ctest ctest --test-dir "$build_dir" --output-on-failure

echo ""
echo "CHECK: PASS"
echo "  $(grep -E '[0-9]+% tests passed' "$log_dir/ctest.log" | tail -n 1)"
