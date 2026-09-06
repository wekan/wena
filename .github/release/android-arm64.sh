#!/usr/bin/env sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
output_dir="$root_dir/dist/android-arm64"
binary="$output_dir/wena"
android_api=21

if test -z "${ANDROID_NDK_ROOT:-}"; then
  echo "Android arm64 toolchain not found (ANDROID_NDK_ROOT is unset)" >&2
  exit 1
fi

toolchain="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin"
compiler="$toolchain/aarch64-linux-android${android_api}-clang"
readelf="$toolchain/llvm-readelf"
if ! test -x "$compiler" || ! test -x "$readelf"; then
  echo "Android arm64 toolchain is incomplete under ANDROID_NDK_ROOT" >&2
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

file "$binary" | grep -Eq 'ELF 64-bit.*ARM aarch64'
"$readelf" -h "$binary" | grep -Eq 'Machine:[[:space:]]+AArch64'
"$readelf" -l "$binary" | grep -Fq '/system/bin/linker64'
