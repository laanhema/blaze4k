#!/usr/bin/env bash
#
# Build the bundled symbol fallback font (#124).
#
# Produces assets/fonts/NotoSansSymbols-Subset.ttf: a merged subset of Noto Sans
# Symbols (1) and Noto Sans Symbols 2 covering the code point blocks the text
# renderer bakes as symbols:
#
#   U+2190-21FF  Arrows
#   U+25A0-25FF  Geometric Shapes
#   U+2600-26FF  Miscellaneous Symbols (U+263A WHITE SMILING FACE is only in Symbols 1)
#   U+2700-27BF  Dingbats
#
# Sources (static, unhinted TTFs, pinned to upstream commits and verified by sha256):
#   https://raw.githubusercontent.com/notofonts/notofonts.github.io/1b2fe62733b83bdb2018a77978be2d7aa424fd43/fonts/NotoSansSymbols/unhinted/ttf/NotoSansSymbols-Regular.ttf
#   https://raw.githubusercontent.com/notofonts/notofonts.github.io/c16b117609abbe4e60b3f2bd4433bdb3d0accb2e/fonts/NotoSansSymbols2/unhinted/ttf/NotoSansSymbols2-Regular.ttf
# License: SIL Open Font License 1.1, copyright The Noto Project Authors
# (https://github.com/notofonts/symbols). No Reserved Font Name is declared.
# The OFL text is copied to assets/fonts/OFL-NotoSansSymbols.txt.
#
# Dev-time only: the script downloads the sources once into a throwaway temp
# dir and builds fonttools in a throwaway venv. The built font is committed;
# the game never downloads anything.
#
# Usage: scripts/build-symbol-font.sh
#
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: scripts/build-symbol-font.sh

Rebuilds assets/fonts/NotoSansSymbols-Subset.ttf and assets/fonts/OFL-NotoSansSymbols.txt
from pinned upstream Noto Sans Symbols 1 + 2 sources. Needs network access, curl,
python3 (with venv) and sha256sum. Nothing is installed system-wide.

  -h, --help      Show this help.
EOF
}

case "${1:-}" in
    "") ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown option: $1" >&2; usage; exit 2 ;;
esac

readonly FONTTOOLS_VERSION='4.66.1'
readonly SYMBOLS1_URL='https://raw.githubusercontent.com/notofonts/notofonts.github.io/1b2fe62733b83bdb2018a77978be2d7aa424fd43/fonts/NotoSansSymbols/unhinted/ttf/NotoSansSymbols-Regular.ttf'
readonly SYMBOLS1_SHA256='6eea9cb4cd39269ea9f95ba5c2735f80ae74049dfc9e1a7c932a5cfc8f0c3030'
readonly SYMBOLS2_URL='https://raw.githubusercontent.com/notofonts/notofonts.github.io/c16b117609abbe4e60b3f2bd4433bdb3d0accb2e/fonts/NotoSansSymbols2/unhinted/ttf/NotoSansSymbols2-Regular.ttf'
readonly SYMBOLS2_SHA256='c4a0a80f0041ce4be81e2478faad22776d23edb98ae3f0d19bd37044820ecf9d'
readonly OFL_URL='https://raw.githubusercontent.com/notofonts/symbols/e8d979919e083f8c60d884f2616d4a15b5ee77e0/OFL.txt'
readonly UNICODES='U+2190-21FF,U+25A0-25FF,U+2600-26FF,U+2700-27BF'

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
out="$repo_root/assets/fonts/NotoSansSymbols-Subset.ttf"
ofl_out="$repo_root/assets/fonts/OFL-NotoSansSymbols.txt"

work="$(mktemp -d "${TMPDIR:-/tmp}/blaze4k-symbol-font.XXXXXX")"
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/dl"

echo "==> Creating throwaway venv (fonttools==$FONTTOOLS_VERSION)"
python3 -m venv "$work/venv"
"$work/venv/bin/pip" install -q "fonttools==$FONTTOOLS_VERSION"

echo "==> Downloading pinned sources"
curl -fsSL -o "$work/dl/NotoSansSymbols-Regular.ttf" "$SYMBOLS1_URL"
curl -fsSL -o "$work/dl/NotoSansSymbols2-Regular.ttf" "$SYMBOLS2_URL"
curl -fsSL -o "$work/dl/OFL.txt" "$OFL_URL"

echo "==> Verifying sha256"
(
    cd "$work/dl"
    printf '%s  %s\n' \
        "$SYMBOLS1_SHA256" NotoSansSymbols-Regular.ttf \
        "$SYMBOLS2_SHA256" NotoSansSymbols2-Regular.ttf | sha256sum -c -
)

echo "==> Subsetting ($UNICODES)"
for name in NotoSansSymbols NotoSansSymbols2; do
    "$work/venv/bin/pyftsubset" "$work/dl/$name-Regular.ttf" --unicodes="$UNICODES" \
        --layout-features='' --drop-tables+=GSUB,GPOS,GDEF --no-hinting --notdef-outline \
        --name-IDs='*' --output-file="$work/$name-subset.ttf"
done

echo "==> Merging (Symbols 1 first, so its glyphs win any overlap)"
SOURCE_DATE_EPOCH=0 "$work/venv/bin/pyftmerge" --output-file="$work/merged.ttf" \
    "$work/NotoSansSymbols-subset.ttf" "$work/NotoSansSymbols2-subset.ttf"

cp "$work/merged.ttf" "$out"
cp "$work/dl/OFL.txt" "$ofl_out"

echo "==> Wrote $out ($(wc -c <"$out") bytes)"
sha256sum "$out"
echo "==> Wrote $ofl_out"
