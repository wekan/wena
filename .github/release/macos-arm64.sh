#!/bin/sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
output_dir="$root_dir/dist/macos-arm64"
binary="$output_dir/wena"

if test "$(uname -s)" != Darwin || ! command -v xcrun >/dev/null 2>&1; then
  echo "macOS arm64 toolchain not found (expected xcrun on Darwin)" >&2
  exit 1
fi

compiler=$(xcrun --sdk macosx --find clang)
# Run by path, clang has no SDK of its own: without -isysroot it finds no
# <stdio.h> (GitHub macos-15 and Command Line Tools alike).
sdk=$(xcrun --sdk macosx --show-sdk-path)
lipo=$(xcrun --find lipo)
mkdir -p "$output_dir"
"$compiler" \
  -isysroot "$sdk" \
  -arch arm64 \
  -std=c89 \
  -pedantic-errors \
  -Wall \
  -Wextra \
  -Werror \
  -O2 \
  "$root_dir/client/main.c" \
  "$root_dir/imports/i18n/catalog.c" \
  -o "$binary"
python3 "$root_dir/scripts/embed_migrations.py" --executable "$binary"
python3 "$root_dir/scripts/embed_i18n_catalog.py" --executable "$binary"

file "$binary" | grep -Eq 'Mach-O 64-bit.*arm64'
test "$("$lipo" -archs "$binary")" = arm64
