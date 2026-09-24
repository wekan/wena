#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/wena-table-XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
 -isystem "$root_dir/third_party/nuklear" \
 "$root_dir/tests/nuklear_paginated_table_test.c" \
 "$root_dir/client/components/common/paginated_table.c" \
 "$root_dir/imports/ui/page_contract.c" "$root_dir/models/color.c" \
 -lm -o "$test_dir/test"
"$test_dir/test"
