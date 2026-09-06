#!/bin/sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
binary="$root_dir/dist/ios-arm64/wena"

"$root_dir/.github/release/ios-arm64.sh"
test -x "$binary"
