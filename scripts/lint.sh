#!/usr/bin/env bash
#
# Repo rules that the compiler does not check. scripts/check.sh runs this
# first, so it runs locally and in CI with the same command. AGENTS.md
# ("Rules and what enforces them") lists each rule next to its check.
#
# Usage: scripts/lint.sh
#
# Every failure prints the rule name, file:line, and what to use instead.
#
# An exception goes on the offending line, as a comment:
#
#   lint-allow(<rule>): <reason>; until YYYY-MM-DD; approved-by <name>
#
# All three parts are required. <name> is the human who approved it. The lint
# fails again when the date has passed.
#
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

# Tracked files plus new ones that are not ignored, so a rule also covers a
# file before its first commit. An exported tree without .git is searched as-is.
if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    search=(git grep --untracked -nE)
else
    search=(git grep --no-index -nE)
fi

failures=0

# rule NAME "what to use instead" PATTERN PATHSPEC...
# Fails for every line under the pathspecs that matches the extended regex.
rule() {
    local name="$1" fix="$2" pattern="$3"
    shift 3
    local hits
    hits="$("${search[@]}" -e "$pattern" -- "$@" | command grep -vF "lint-allow($name)" || true)"
    if [ -n "$hits" ]; then
        echo "LINT: $name"
        echo "$hits" | sed 's/^/  /'
        echo "  Fix: $fix"
        echo ""
        failures=$((failures + 1))
    fi
}

# Every lint-allow must name a rule, a reason, an expiry date that has not
# passed, and the human who approved it.
check_exceptions() {
    local today hits bad="" line date
    today="$(date +%Y-%m-%d)"
    hits="$("${search[@]}" -e 'lint-allow\(' -- src tests scripts docs '*.md' '*.txt' \
        ':!scripts/lint.sh' ':!.agents' || true)"
    while IFS= read -r line; do
        [ -z "$line" ] && continue
        if [[ "$line" =~ lint-allow\([a-z-]+\):\ [^\;]+\;\ until\ ([0-9]{4}-[0-9]{2}-[0-9]{2})\;\ approved-by\ [^[:space:]]+ ]]; then
            date="${BASH_REMATCH[1]}"
            if [[ "$date" < "$today" ]]; then
                bad+="  $line"$'\n'"    (expired $date)"$'\n'
            fi
        else
            bad+="  $line"$'\n'
        fi
    done <<<"$hits"
    if [ -n "$bad" ]; then
        echo "LINT: lint-exception"
        printf '%s' "$bad"
        echo "  Fix: remove the exception, or write it in full with a date that has not passed:"
        echo "       lint-allow(<rule>): <reason>; until YYYY-MM-DD; approved-by <name>"
        echo ""
        failures=$((failures + 1))
    fi
}

# --- Rules -------------------------------------------------------------------

# The audio engine opens the silent null device unless it is asked for the real
# one (AudioBackend in src/audio/audio_engine.hpp). Only the game and the
# latency probe may ask.
rule audio-device \
    "leave AudioEngineSettings::backend at its default (the silent null device). Only src/main.cpp and tests/clock_probe.cpp may open the real sound device." \
    'AudioBackend::Device' \
    src tests ':!src/main.cpp' ':!tests/clock_probe.cpp'

# AudioEngine is the one owner of the output device, so that default holds.
rule audio-device-open \
    "play sound through AudioEngine / SoundStream (src/audio). Only src/audio/audio_engine.cpp may open a miniaudio device." \
    'ma_(engine|device|context)_init\(' \
    src tests ':!src/audio/audio_engine.cpp'

# Text from a simfile, the command line or a config is untrusted. The raw
# conversions throw, abort, accept trailing junk or return NaN, each in its own
# way; src/util/parse_number.hpp is the one checked path.
rule number-parsing \
    "use src/util/parse_number.hpp: parse_double / parse_int for a whole value (CLI, config), parse_leading_finite / parse_leading_int for a simfile field." \
    '(std::sto(i|l|ll|ul|ull|f|d|ld)|[^a-z_](ato[ifl]|atoll|strto(l|ll|ul|ull|d|f|ld|imax|umax))|sscanf|from_chars) *\(' \
    src ':!src/util/parse_number.cpp'

check_exceptions

if [ "$failures" -ne 0 ]; then
    echo "LINT: FAIL ($failures rule(s))"
    exit 1
fi
echo "LINT: PASS"
