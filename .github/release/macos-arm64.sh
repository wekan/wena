#!/bin/sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
output_dir="$root_dir/dist/macos-arm64"
binary="$output_dir/wena"

if test "$(uname -s)" != Darwin || ! command -v xcrun >/dev/null 2>&1; then
  echo "macOS arm64 toolchain not found (expected xcrun on Darwin)" >&2
  exit 1
fi

compiler=$(xcrun --find clang)
lipo=$(xcrun --find lipo)
mkdir -p "$output_dir"
"$compiler" \
  -arch arm64 \
  -std=c89 \
  -pedantic-errors \
  -Wall \
  -Wextra \
  -Werror \
  -O2 \
  "$root_dir/src/main.c" \
  -o "$binary"

file "$binary" | grep -Eq 'Mach-O 64-bit.*arm64'
test "$("$lipo" -archs "$binary")" = arm64
