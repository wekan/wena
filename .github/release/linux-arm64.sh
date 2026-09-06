#!/usr/bin/env sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
output_dir="$root_dir/dist/linux-arm64"

if command -v aarch64-linux-gnu-gcc >/dev/null 2>&1; then
  compiler=aarch64-linux-gnu-gcc
elif test "$(uname -m)" = aarch64 && command -v gcc >/dev/null 2>&1; then
  # Native ARM64 builds make it possible to verify the same artifact locally.
  compiler=gcc
else
  echo "Linux arm64 compiler not found (expected aarch64-linux-gnu-gcc)" >&2
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
  -o "$output_dir/wena"
