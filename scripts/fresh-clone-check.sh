#!/usr/bin/env bash
#
# Fresh-clone verification (Linux leg of AC1 / PRD section 11).
#
# Exports a clean source tree (no committed/ignored build artifacts), then
# configures, builds, and runs the full ctest suite from that tree. By default
# it reuses the local build/_deps cache via FETCHCONTENT_BASE_DIR so the run is
# offline and deterministic; pass --online to exercise a real FetchContent fetch.
#
# Usage: scripts/fresh-clone-check.sh [--online] [--build-dir <path>] [--committed] [--keep]
#
#   --online        Fetch dependencies fresh (no local build/_deps reuse).
#   --build-dir P   Build directory (default: <fresh-tree>/build).
#   --committed     Export committed HEAD only (post-commit use). The default
#                   exports the current working tree (tracked + untracked,
#                   honoring .gitignore) so uncommitted changes are covered too.
#   --keep          Keep the temporary export tree and logs even on success.
#
# Linux/GNU-tar leg: the export uses GNU tar options (`--null`,
# `--ignore-failed-read`); run it on Linux with GNU tar.
#
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: scripts/fresh-clone-check.sh [--online] [--build-dir <path>] [--committed] [--keep]

  --online        Fetch dependencies fresh (no local build/_deps reuse).
  --build-dir P   Build directory (default: <fresh-tree>/build).
  --committed     Export committed HEAD only (post-commit use).
  --keep          Keep the temporary export tree and logs (default: removed
                  on success; always kept on failure for inspection).
  -h, --help      Show this help.

Requires GNU tar (Linux leg; the export uses GNU tar options).
EOF
}

online=0
committed=0
keep=0
build_dir=""
while [ $# -gt 0 ]; do
    case "$1" in
        --online) online=1; shift ;;
        --committed) committed=1; shift ;;
        --keep) keep=1; shift ;;
        --build-dir)
            build_dir="${2:?--build-dir requires a path}"
            shift 2
            ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage; exit 2 ;;
    esac
done

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
work="$(mktemp -d "${TMPDIR:-/tmp}/blaze4k-fresh-clone.XXXXXX")"
tree="$work/src"
log_dir="$work/logs"
mkdir -p "$tree" "$log_dir"

# Keep the tree/logs on failure (for the tail dump below) and whenever --keep is
# passed; remove the temp tree on a clean success otherwise.
cleanup() {
    local status=$?
    if [ "$keep" -eq 1 ] || [ "$status" -ne 0 ]; then
        return
    fi
    rm -rf "$work"
}
trap cleanup EXIT

# 1. Export a clean tree (never the committed/ignored build/ artifacts).
if [ "$committed" -eq 1 ]; then
    git -C "$repo_root" archive HEAD | tar -x -C "$tree"
else
    git -C "$repo_root" ls-files -z --cached --others --exclude-standard \
        | tar -C "$repo_root" --null --ignore-failed-read -cf - -T - \
        | tar -x -C "$tree"
fi

if [ -z "$build_dir" ]; then
    build_dir="$tree/build"
fi
mkdir -p "$build_dir"

jobs="$(nproc 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"

fail() {
    echo ""
    echo "FRESH-CLONE CHECK: FAIL"
    echo "  step: $1"
    echo "  log:  $2"
    echo "  tree: $tree"
    echo "  ----- last 20 log lines -----"
    tail -n 20 "$2" || true
    exit 1
}

echo "Fresh tree: $tree"
echo "Build dir:  $build_dir"

cmake_args=(-S "$tree" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release)
if [ "$online" -eq 1 ]; then
    echo "Dependency mode: online (fresh FetchContent download)"
else
    cmake_args+=("-DFETCHCONTENT_BASE_DIR=$repo_root/build/_deps")
    echo "Dependency mode: offline (reuse $repo_root/build/_deps when present)"
fi

echo "[1/3] Configure..."
if ! cmake "${cmake_args[@]}" >"$log_dir/configure.log" 2>&1; then
    fail "configure" "$log_dir/configure.log"
fi

echo "[2/3] Build (-j$jobs)..."
if ! cmake --build "$build_dir" -j"$jobs" >"$log_dir/build.log" 2>&1; then
    fail "build" "$log_dir/build.log"
fi

echo "[3/3] Test..."
if ! ctest --test-dir "$build_dir" --output-on-failure >"$log_dir/ctest.log" 2>&1; then
    fail "ctest" "$log_dir/ctest.log"
fi

passed_line="$(grep -E '[0-9]+% tests passed out of [0-9]+' "$log_dir/ctest.log" | tail -n 1 || true)"
if ! [[ "$passed_line" =~ ^100%[[:space:]]tests[[:space:]]passed[[:space:]]out[[:space:]]of[[:space:]]([1-9][0-9]*) ]]; then
    fail "test-suite (expected 100% of at least one test; got: ${passed_line:-unknown})" "$log_dir/ctest.log"
fi

echo ""
echo "FRESH-CLONE CHECK: PASS"
echo "  $passed_line"
if [ "$keep" -eq 1 ]; then
    echo "  tree: $tree"
    echo "  logs: $log_dir"
else
    echo "  temp tree removed (pass --keep to retain it)"
fi
