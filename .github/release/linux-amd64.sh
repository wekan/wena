#!/usr/bin/env sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
output_dir="$root_dir/dist/linux-amd64"
binary="$output_dir/wena"

if command -v x86_64-linux-gnu-gcc >/dev/null 2>&1; then
  compiler=x86_64-linux-gnu-gcc
elif test "$(uname -m)" = x86_64 && command -v gcc >/dev/null 2>&1; then
  compiler=gcc
else
  echo "Linux amd64 compiler not found (expected x86_64-linux-gnu-gcc)" >&2
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

file "$binary" | grep -Eq 'ELF 64-bit.*(x86-64|x86_64)'
readelf -h "$binary" | grep -Eq 'Machine:[[:space:]]+(Advanced Micro Devices X86-64|X86-64)'
