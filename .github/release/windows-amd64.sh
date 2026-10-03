#!/usr/bin/env sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
output_dir="$root_dir/dist/windows-amd64"
binary="$output_dir/wena.exe"

if command -v x86_64-w64-mingw32-gcc >/dev/null 2>&1 &&
   command -v x86_64-w64-mingw32-objdump >/dev/null 2>&1; then
  compiler=x86_64-w64-mingw32-gcc
  objdump=x86_64-w64-mingw32-objdump
elif command -v gcc >/dev/null 2>&1 && command -v objdump >/dev/null 2>&1 &&
     test "$(gcc -dumpmachine)" = x86_64-w64-mingw32; then
  # On Windows, a native MinGW-w64 gcc builds the same artifact.
  compiler=gcc
  objdump=objdump
else
  echo "Windows amd64 compiler not found (expected x86_64-w64-mingw32-gcc and -objdump)" >&2
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
  "$root_dir/imports/i18n/catalog.c" \
  -o "$binary"
python3 "$root_dir/scripts/embed_migrations.py" --executable "$binary"
python3 "$root_dir/scripts/embed_i18n_catalog.py" --executable "$binary"

file "$binary" | grep -Eq 'PE32\+ executable.*(x86-64.*Windows|Windows.*x86-64)'
"$objdump" -f "$binary" |
  grep -Eq 'architecture: i386:x86-64'
