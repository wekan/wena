#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/wena-directory-XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
python3 "$root_dir/scripts/embed_test_files.py" "$test_dir/wena_test_files.h" \
  "$root_dir/server/migrations/001_initial.sql"
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror -I"$test_dir" \
 "$root_dir/tests/support/test_files.c" "$root_dir/tests/sqlite_directory_test.c" "$root_dir/server/sqlite_directory.c" \
 "$root_dir/models/directory.c" "$root_dir/models/model.c" -lsqlite3 -o "$test_dir/test"
"$test_dir/test" "$root_dir/server/migrations/001_initial.sql" "$test_dir/directory.sqlite"
