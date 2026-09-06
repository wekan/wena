#!/usr/bin/env sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
binary="$root_dir/dist/linux-amd64/wena"

"$root_dir/.github/release/linux-amd64.sh"
test -x "$binary"
