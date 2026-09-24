#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/wena-ferretdb-scan-XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
 "$root_dir/tests/ferretdb_scan_test.c" "$root_dir/server/ferretdb_scan.c" \
 "$root_dir/server/ferretdb_compat.c" "$root_dir/imports/ferretdb/sjson.c" \
 "$root_dir/imports/json/document.c" "$root_dir/models/text.c" -lsqlite3 -o "$test_dir/test"
"$test_dir/test" "$test_dir"
