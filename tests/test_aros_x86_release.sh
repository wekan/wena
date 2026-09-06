#!/usr/bin/env sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
binary="$root_dir/dist/aros-x86/wena"

"$root_dir/.github/release/aros-x86.sh"
test -x "$binary"
