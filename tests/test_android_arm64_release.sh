#!/usr/bin/env sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
binary="$root_dir/dist/android-arm64/wena"

"$root_dir/.github/release/android-arm64.sh"
test -x "$binary"
