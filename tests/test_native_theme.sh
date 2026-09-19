#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/wena-native-theme-XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
  -I"$root_dir/third_party/nuklear" \
  "$root_dir/tests/native_theme_test.c" "$root_dir/client/platform/theme.c" \
  "$root_dir/imports/ui/page_contract.c" "$root_dir/models/color.c" -o "$test_dir/test" -lm
"$test_dir/test"
