#!/usr/bin/env sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
binary="$root_dir/dist/linux-armhf/wena"

"$root_dir/.github/release/linux-armhf.sh"
test -x "$binary"
