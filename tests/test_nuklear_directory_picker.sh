#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/wena-directory-picker-XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror -isystem "$root_dir/third_party/nuklear" \
 "$root_dir/tests/nuklear_directory_picker_test.c" "$root_dir/client/features/directory_picker.c" \
 "$root_dir/client/components/common/paginated_table.c" "$root_dir/server/sqlite_directory.c" \
 "$root_dir/models/directory.c" "$root_dir/models/model.c" "$root_dir/models/color.c" \
 "$root_dir/imports/ui/page_contract.c" -lsqlite3 -lm -o "$test_dir/test"
"$test_dir/test" "$root_dir/server/migrations/001_initial.sql"
