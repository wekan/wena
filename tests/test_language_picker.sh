#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/wena-language-picker-XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
  -I"$root_dir/third_party/nuklear" \
  "$root_dir/tests/language_picker_test.c" \
  "$root_dir/client/features/language_picker.c" \
  "$root_dir/imports/i18n/ui_catalog.c" "$root_dir/imports/i18n/locale.c" \
  "$root_dir/imports/i18n/language.c" "$root_dir/imports/ui/page_contract.c" "$root_dir/models/color.c" \
  -o "$test_dir/test" -lm
"$test_dir/test" "$test_dir"
