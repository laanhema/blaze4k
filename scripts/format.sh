#!/usr/bin/env bash
#
# Check or format changed lines using clang-format and git-clang-format.
# scripts/check.sh runs this with --check as its second step.
#
# Usage:
#   scripts/format.sh            # Formats changed lines in place
#   scripts/format.sh --check    # Fails if any changed lines are unformatted
#   scripts/format.sh <commit>   # Formats lines changed since <commit>
#   scripts/format.sh --check <commit>
#
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

if [ -d "$HOME/.local/bin" ]; then
    case ":$PATH:" in
        *":$HOME/.local/bin:"*) ;;
        *) PATH="$HOME/.local/bin:$PATH" ;;
    esac
fi

check_only=0
base_arg=""

while [ $# -gt 0 ]; do
    case "$1" in
        --check) check_only=1; shift ;;
        -h|--help)
            cat <<'USAGE'
Usage: scripts/format.sh [--check] [<base-commit>]

  --check         Check only; exit 1 if unformatted lines exist.
  <base-commit>   Base revision to diff against (default: auto-detected merge base).
  -h, --help      Show this help.
USAGE
            exit 0
            ;;
        *)
            if [ -z "$base_arg" ]; then
                base_arg="$1"
                shift
            else
                echo "Unknown argument: $1" >&2
                exit 2
            fi
            ;;
    esac
done

if ! command -v git-clang-format >/dev/null 2>&1 && ! git clang-format -h >/dev/null 2>&1; then
    echo "FORMAT: clang-format"
    echo "  git-clang-format not found in PATH"
    echo "  Fix: install clang-format 23.1.1 (pip install clang-format==23.1.1)"
    echo ""
    exit 1
fi

if ! git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    exit 0
fi

if [ -n "$base_arg" ]; then
    base="$base_arg"
elif [ -n "${GITHUB_BASE_REF:-}" ]; then
    base="$(git merge-base HEAD "origin/$GITHUB_BASE_REF" 2>/dev/null || git merge-base HEAD "$GITHUB_BASE_REF" 2>/dev/null || echo "origin/$GITHUB_BASE_REF")"
elif [ "${GITHUB_EVENT_NAME:-}" = "push" ] && git rev-parse HEAD~1 >/dev/null 2>&1; then
    base="HEAD~1"
else
    base="$(git merge-base HEAD origin/main 2>/dev/null || git merge-base HEAD main 2>/dev/null || echo "HEAD")"
fi

[ -z "$base" ] && base="HEAD"

if [ "$base" = "HEAD" ]; then
    cmd_args=(HEAD)
else
    cmd_args=("$base")
fi

if [ "$check_only" -eq 1 ]; then
    set +e
    output="$(git clang-format --diff "${cmd_args[@]}" 2>&1)"
    ret=$?
    set -e

    if [ "$ret" -eq 1 ] && echo "$output" | grep -q "^diff --git"; then
        files="$(echo "$output" | grep "^diff --git" | awk '{print $3}' | sed 's|^a/||' | sort -u)"
        echo "FORMAT: clang-format"
        echo "$files" | sed 's/^/  /'
        echo "  Fix: scripts/format.sh"
        echo ""
        exit 1
    elif [ "$ret" -eq 0 ]; then
        echo "FORMAT: PASS"
        exit 0
    else
        echo "FORMAT: clang-format"
        echo "  git-clang-format failed with exit code $ret"
        if [ -n "$output" ]; then
            echo "$output" | sed 's/^/  /'
        fi
        echo ""
        exit "$ret"
    fi
else
    set +e
    git clang-format -f "${cmd_args[@]}"
    ret=$?
    set -e
    if [ "$ret" -eq 1 ]; then
        # git-clang-format exits 1 when files were modified (designed for pre-commit hooks).
        # When running as an in-place formatter, modifications are the intended outcome.
        exit 0
    elif [ "$ret" -ne 0 ]; then
        exit "$ret"
    fi
fi
