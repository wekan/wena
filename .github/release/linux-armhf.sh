#!/usr/bin/env sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
output_dir="$root_dir/dist/linux-armhf"
binary="$output_dir/wena"

if command -v arm-linux-gnueabihf-gcc >/dev/null 2>&1; then
  compiler=arm-linux-gnueabihf-gcc
elif case "$(uname -m)" in armv7l|armv8l) true;; *) false;; esac &&
     command -v gcc >/dev/null 2>&1; then
  compiler=gcc
else
  echo "Linux armhf compiler not found (expected arm-linux-gnueabihf-gcc)" >&2
  exit 1
fi

mkdir -p "$output_dir"
"$compiler" \
  -std=c89 \
  -pedantic-errors \
  -Wall \
  -Wextra \
  -Werror \
  -O2 \
  "$root_dir/client/main.c" \
  -o "$binary"

file "$binary" | grep -Eq 'ELF 32-bit.*ARM'
readelf -h "$binary" | grep -Eq 'Class:[[:space:]]+ELF32'
readelf -h "$binary" | grep -Eq 'Machine:[[:space:]]+ARM'
readelf -h "$binary" | grep -Eq 'Flags:.*hard-float ABI'
