#!/bin/sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
output_dir="$root_dir/dist/ios-arm64"
binary="$output_dir/wena"
deployment_target=13.0

if test "$(uname -s)" != Darwin || ! command -v xcrun >/dev/null 2>&1; then
  echo "iOS arm64 toolchain not found (expected xcrun on Darwin)" >&2
  exit 1
fi
if ! xcrun --sdk iphoneos --show-sdk-path >/dev/null 2>&1; then
  echo "iOS arm64 SDK not found (expected the iphoneos SDK)" >&2
  exit 1
fi

compiler=$(xcrun --sdk iphoneos --find clang)
lipo=$(xcrun --find lipo)
vtool=$(xcrun --find vtool)
mkdir -p "$output_dir"
"$compiler" \
  -arch arm64 \
  -mios-version-min="$deployment_target" \
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

file "$binary" | grep -Eq 'Mach-O 64-bit.*arm64'
test "$("$lipo" -archs "$binary")" = arm64
"$vtool" -show-build "$binary" | grep -Eq 'platform[[:space:]]+IOS'
