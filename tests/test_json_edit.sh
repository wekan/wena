#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/wena-json-edit-XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
 "$root_dir/tests/json_edit_test.c" "$root_dir/imports/json/edit.c" \
 "$root_dir/imports/ferretdb/edit.c" "$root_dir/imports/ferretdb/sjson.c" \
 "$root_dir/imports/json/document.c" "$root_dir/models/text.c" -o "$test_dir/test"
"$test_dir/test"
