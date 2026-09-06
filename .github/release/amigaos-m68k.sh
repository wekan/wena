#!/usr/bin/env sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
output_dir="$root_dir/dist/amigaos-m68k"
binary="$output_dir/wena"
toolchain_image=amigadev/crosstools:m68k-amigaos-gcc10

if ! command -v docker >/dev/null 2>&1; then
  echo "AmigaOS m68k toolchain not found (expected Docker)" >&2
  exit 1
fi

mkdir -p "$output_dir"
docker run --rm \
  --user "$(id -u):$(id -g)" \
  --volume "$root_dir:/work" \
  --workdir /work \
  "$toolchain_image" \
  m68k-amigaos-gcc \
  -noixemul \
  -m68000 \
  -std=c89 \
  -pedantic-errors \
  -Wall \
  -Wextra \
  -Werror \
  -O2 \
  client/main.c \
  imports/i18n/catalog.c \
  -o dist/amigaos-m68k/wena
python3 "$root_dir/scripts/embed_i18n_catalog.py" --executable "$binary"

file "$binary" | grep -Fq 'AmigaOS loadseg()ble executable/binary'
test "$(od -An -tx1 -N4 "$binary" | tr -d ' \n')" = 000003f3
