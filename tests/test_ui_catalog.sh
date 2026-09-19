#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/wena-ui-catalog-XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
python3 "$root_dir/scripts/generate_ui_i18n.py" --check
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
  "$root_dir/tests/ui_catalog_test.c" "$root_dir/imports/i18n/ui_catalog.c" \
  "$root_dir/imports/i18n/locale.c" "$root_dir/imports/i18n/language.c" \
  "$root_dir/imports/ui/page_contract.c" "$root_dir/models/color.c" \
  -o "$test_dir/test"
python3 "$root_dir/tests/test_ui_catalog.py" "$test_dir/test" "$test_dir/language.conf"
