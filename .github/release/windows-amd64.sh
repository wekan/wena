#!/usr/bin/env sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
output_dir="$root_dir/dist/windows-amd64"
binary="$output_dir/wena.exe"

if ! command -v x86_64-w64-mingw32-gcc >/dev/null 2>&1; then
  echo "Windows amd64 compiler not found (expected x86_64-w64-mingw32-gcc)" >&2
  exit 1
fi
if ! command -v x86_64-w64-mingw32-objdump >/dev/null 2>&1; then
  echo "Windows amd64 objdump not found (expected x86_64-w64-mingw32-objdump)" >&2
  exit 1
fi

mkdir -p "$output_dir"
x86_64-w64-mingw32-gcc \
  -std=c89 \
  -pedantic-errors \
  -Wall \
  -Wextra \
  -Werror \
  -O2 \
  "$root_dir/client/main.c" \
  "$root_dir/imports/i18n/catalog.c" \
  -o "$binary"
python3 "$root_dir/scripts/embed_i18n_catalog.py" --executable "$binary"

file "$binary" | grep -Eq 'PE32\+ executable.*x86-64.*Windows'
x86_64-w64-mingw32-objdump -f "$binary" |
  grep -Eq 'architecture: i386:x86-64'
