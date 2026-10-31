#!/bin/sh
# Downloads pinned upstream emulator sources into a cache; no ROMs are vendored.
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cache=${A2CALC_EMU_CACHE:-/tmp/a2calc-emu-build}
source_dir="$cache/apple2js"
mkdir -p "$cache" "$repo_dir/build"
if [ ! -f "$source_dir/js/mmu.ts" ]; then
    git clone https://github.com/whscullin/apple2js.git "$source_dir"
fi
git -C "$source_dir" checkout --detach ee0aed25f73c69d0245e86a2a5fccb3324c3056c
git -C "$source_dir" submodule update --init submodules/cpu6502
if [ ! -x "$cache/node_modules/.bin/esbuild" ]; then
    npm install --prefix "$cache" --no-audit --no-fund esbuild@0.28.2
fi
"$cache/node_modules/.bin/esbuild" "$repo_dir/tools/emulator-smoke.ts" \
    --bundle --platform=node --format=cjs \
    --alias:emu="$source_dir" \
    --alias:@whscullin/cpu6502="$source_dir/submodules/cpu6502/js/index.ts" \
    --outfile="$cache/emulator.cjs"
cd "$repo_dir"
node "$cache/emulator.cjs" "${1:-dist/A2CALC.dsk}" "${2:-dist/A2DATA.dsk}"
