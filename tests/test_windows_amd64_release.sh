#!/usr/bin/env sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
binary="$root_dir/dist/windows-amd64/wena.exe"

"$root_dir/.github/release/windows-amd64.sh"
test -f "$binary"
