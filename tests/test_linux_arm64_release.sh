#!/usr/bin/env sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
binary="$root_dir/dist/linux-arm64/wena"

"$root_dir/.github/release/linux-arm64.sh"

test -x "$binary"
file "$binary" | grep -Eq 'ELF 64-bit.*(ARM aarch64|ARM64)'
readelf -h "$binary" | grep -Eq 'Machine:[[:space:]]+AArch64'
