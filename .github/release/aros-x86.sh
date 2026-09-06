#!/usr/bin/env sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
output_dir="$root_dir/dist/aros-x86"
binary="$output_dir/wena"
toolchain_image='amigadev/crosstools:x86_64-aros@sha256:9c4e978301da6b6584d68d4014caa1fa9e2689529e79e71f7190d40d1cb62e50'

if ! command -v docker >/dev/null 2>&1; then
  echo "AROS x86 toolchain not found (expected Docker)" >&2
  exit 1
fi

mkdir -p "$output_dir"
test "$(docker run --rm "$toolchain_image" x86_64-aros-gcc -dumpmachine)" = x86_64-aros
docker run --rm \
  --user "$(id -u):$(id -g)" \
  --volume "$root_dir:/work" \
  --workdir /work \
  "$toolchain_image" \
  x86_64-aros-gcc \
  -std=c89 \
  -pedantic-errors \
  -Wall \
  -Wextra \
  -Werror \
  -O2 \
  client/main.c \
  imports/i18n/catalog.c \
  -o dist/aros-x86/wena
python3 "$root_dir/scripts/embed_migrations.py" --executable "$binary"
python3 "$root_dir/scripts/embed_i18n_catalog.py" --executable "$binary"

file "$binary" | grep -Eq 'ELF 64-bit.*x86-64'
readelf -h "$binary" | grep -Eq 'Machine:[[:space:]]+Advanced Micro Devices X86-64'
