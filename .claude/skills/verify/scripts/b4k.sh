#!/usr/bin/env bash
# Drive a real Blaze 4k window on the local X11 display for verification.
# Every instance lives in its own run dir; see ../SKILL.md for the workflow.
set -euo pipefail

HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO=$(git -C "$HERE" rev-parse --show-toplevel)
BIN="$REPO/build/blaze-4k"
FIXTURE_SONGS="$REPO/tests/fixtures/reference_pack"

usage() {
    cat <<EOF
usage:
  b4k.sh doctor [RUN]                 environment, build freshness, and (with RUN) instance health
  b4k.sh launch [--run RUN] [-- GAME_ARGS...]
                                      start an isolated instance; prints RUN
  b4k.sh keys RUN TOKEN...            send keys to RUN's window (no focus needed)
                                      TOKEN: Return | Escape | Tab | Left | Down | Up | Right | <keysym>
                                             +Key (press only)  -Key (release only)  wait:SECONDS
  b4k.sh screen RUN                   print the active screen (Title, Select, Gameplay, Results, ...)
  b4k.sh wait-screen RUN NAME [SECS]  block until NAME is active (default 15 s)
  b4k.sh shot RUN NAME                capture the window to RUN/evidence/NAME.png
  b4k.sh stop RUN                     close RUN's instance, keep RUN/evidence
EOF
}

die() { echo "FAIL: $*" >&2; exit 1; }

run_pid() { cat "$1/pid" 2>/dev/null || true; }
run_wid() { cat "$1/wid" 2>/dev/null || true; }

alive() {
    local pid
    pid=$(run_pid "$1")
    [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null &&
        [[ "$(readlink -f "/proc/$pid/exe" 2>/dev/null)" == "$(readlink -f "$BIN")" ]]
}

current_screen() {
    grep -oE '^\[ScreenManager\] (enter [A-Za-z]+|[A-Za-z]+ -> [A-Za-z]+)' "$1/game.log" 2>/dev/null |
        tail -1 | awk '{print $NF}'
}

i3_running() { command -v i3-msg >/dev/null && i3-msg -t get_version >/dev/null 2>&1; }

check_env() {
    local ok=0
    [[ -n "${DISPLAY:-}" ]] && echo "ok   DISPLAY=$DISPLAY" || { echo "FAIL DISPLAY unset (needs an X11 session)"; ok=1; }
    for tool in python3 wmctrl import stdbuf; do
        command -v "$tool" >/dev/null && echo "ok   $tool" || { echo "FAIL $tool missing"; ok=1; }
    done
    python3 -c 'import Xlib' 2>/dev/null && echo "ok   python Xlib" || { echo "FAIL python Xlib missing (pip install --user python-xlib)"; ok=1; }
    if [[ -x "$BIN" ]]; then
        local newer
        newer=$(find "$REPO/src" "$REPO/assets" "$REPO/CMakeLists.txt" -newer "$BIN" -type f -print -quit)
        [[ -z "$newer" ]] && echo "ok   build/blaze-4k is newer than src/ and assets/" ||
            { echo "FAIL build/blaze-4k is stale ($newer is newer); run: cmake --build build -j"; ok=1; }
    else
        echo "FAIL $BIN missing; run: cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j"
        ok=1
    fi
    return $ok
}

check_run() {
    local run=$1 ok=0 wid
    [[ -d "$run" ]] || { echo "FAIL run dir $run missing"; return 1; }
    alive "$run" && echo "ok   pid $(run_pid "$run") is our build/blaze-4k" || { echo "FAIL instance not running"; ok=1; }
    wid=$(run_wid "$run")
    if [[ -n "$wid" ]] && wmctrl -lp | awk -v w="$wid" -v p="$(run_pid "$run")" \
        'strtonum($1) == strtonum(w) && $3 == p { found = 1 } END { exit !found }'; then
        echo "ok   window $wid belongs to the instance"
    else
        echo "FAIL window ${wid:-?} not found for the instance"
        ok=1
    fi
    echo "info screen: $(current_screen "$run")"
    if grep -E 'Failed to|\[main\] Invalid|Music file not found' "$run/game.log" >/dev/null 2>&1; then
        echo "warn game.log has errors:"
        grep -E 'Failed to|\[main\] Invalid|Music file not found' "$run/game.log" | sed 's/^/     /'
    fi
    return $ok
}

cmd_launch() {
    local run=""
    if [[ "${1:-}" == "--run" ]]; then run=$2; shift 2; fi
    [[ "${1:-}" == "--" ]] && shift
    run=${run:-/tmp/blaze4k-verify/$(date -u +%Y%m%dT%H%M%SZ)}
    check_env >/dev/null || { check_env; die "environment not ready"; }
    alive "$run" && die "$run already has a running instance; stop it or pick another --run"
    mkdir -p "$run/data" "$run/evidence"
    : > "$run/game.log"

    local prev
    prev=$(xprop -root _NET_ACTIVE_WINDOW 2>/dev/null | awk '{print $NF}')

    # cwd=build so the bundled assets/ next to the binary resolve; the fixture
    # pack comes first so a caller's own --songs (last one wins) overrides it.
    (cd "$REPO/build" && exec stdbuf -oL -eL "$BIN" --data-dir "$run/data" --songs "$FIXTURE_SONGS" \
        --attract-timeout 0 "$@" >"$run/game.log" 2>&1) &
    echo $! > "$run/pid"

    local wid="" i
    for i in $(seq 100); do
        wid=$(wmctrl -lp | awk -v p="$(run_pid "$run")" '$3 == p { print $1; exit }')
        [[ -n "$wid" ]] && break
        alive "$run" || { cat "$run/game.log" >&2; die "instance exited during startup"; }
        sleep 0.15
    done
    [[ -n "$wid" ]] || die "no window appeared within 15 s"
    echo "$wid" > "$run/wid"

    # A floating 1280x720 sticky window keeps screenshots at the design size and
    # visible even when the user switches i3 workspaces.
    if i3_running; then
        i3-msg "[id=$((wid))] floating enable, resize set 1280 720, move position center, sticky enable" >/dev/null
    fi
    [[ -n "$prev" && "$prev" != "0x0" ]] && wmctrl -i -a "$prev" 2>/dev/null || true

    for i in $(seq 100); do
        [[ -n "$(current_screen "$run")" ]] && break
        sleep 0.1
    done
    echo "run=$run pid=$(run_pid "$run") wid=$wid screen=$(current_screen "$run")"
}

cmd_keys() {
    local run=$1; shift
    alive "$run" || die "instance in $run is not running"
    python3 "$HERE/sendkey.py" "$(run_wid "$run")" "$@"
}

cmd_wait_screen() {
    local run=$1 name=$2 secs=${3:-15} i
    for i in $(seq $((secs * 10))); do
        [[ "$(current_screen "$run")" == "$name" ]] && { echo "screen=$name"; return 0; }
        alive "$run" || die "instance exited while waiting for $name"
        sleep 0.1
    done
    die "screen is $(current_screen "$run"), not $name, after ${secs}s"
}

cmd_shot() {
    local run=$1 name=$2
    alive "$run" || die "instance in $run is not running"
    import -window "$(run_wid "$run")" "$run/evidence/$name.png"
    echo "$run/evidence/$name.png"
}

cmd_stop() {
    local run=$1 pid i
    pid=$(run_pid "$run")
    if alive "$run"; then
        wmctrl -i -c "$(run_wid "$run")" 2>/dev/null || true
        for i in $(seq 100); do kill -0 "$pid" 2>/dev/null || break; sleep 0.1; done
        if kill -0 "$pid" 2>/dev/null; then
            echo "warn: no clean exit after 10 s; killing pid $pid" >&2
            kill "$pid" 2>/dev/null || true
            sleep 1
            kill -9 "$pid" 2>/dev/null || true
        fi
    fi
    mkdir -p "$run/evidence"
    [[ -f "$run/game.log" ]] && cp "$run/game.log" "$run/evidence/game.log"
    for f in config.json scores.json; do
        [[ -f "$run/data/$f" ]] && cp "$run/data/$f" "$run/evidence/$f"
    done
    rm -rf "$run/data" "$run/pid" "$run/wid"
    echo "stopped; evidence kept in $run/evidence"
    ls "$run/evidence"
}

[[ $# -ge 1 ]] || { usage; exit 2; }
cmd=$1; shift
case "$cmd" in
    doctor)
        status=0
        check_env || status=1
        [[ $# -ge 1 ]] && { check_run "$1" || status=1; }
        exit $status ;;
    launch) cmd_launch "$@" ;;
    keys) [[ $# -ge 2 ]] || { usage; exit 2; }; cmd_keys "$@" ;;
    screen) [[ $# -eq 1 ]] || { usage; exit 2; }; current_screen "$1" ;;
    wait-screen) [[ $# -ge 2 ]] || { usage; exit 2; }; cmd_wait_screen "$@" ;;
    shot) [[ $# -eq 2 ]] || { usage; exit 2; }; cmd_shot "$@" ;;
    stop) [[ $# -eq 1 ]] || { usage; exit 2; }; cmd_stop "$1" ;;
    -h|--help|help) usage ;;
    *) usage; exit 2 ;;
esac
