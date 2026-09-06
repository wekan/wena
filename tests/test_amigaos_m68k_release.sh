#!/usr/bin/env sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
binary="$root_dir/dist/amigaos-m68k/wena"

"$root_dir/.github/release/amigaos-m68k.sh"
test -x "$binary"
