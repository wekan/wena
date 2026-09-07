#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/wena-native-font-XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
python3 "$root_dir/scripts/generate_native_font.py" --check
python3 "$root_dir/tests/test_native_font.py"
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
  -I"$root_dir/third_party/nuklear" \
  "$root_dir/tests/native_font_test.c" "$root_dir/client/platform/font.c" \
  -o "$test_dir/test" -lm
"$test_dir/test"
