#!/bin/sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
binary="$root_dir/dist/macos-amd64/wena"

"$root_dir/.github/release/macos-amd64.sh"
test -x "$binary"
