#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir="${TMPDIR:-/tmp}/wena-sqlite-storage-test-$$"
mkdir -p "$test_dir"
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
  "$root_dir/tests/sqlite_storage_test.c" "$root_dir/server/sqlite_storage.c" \
  "$root_dir/server/sha256.c" -lsqlite3 -o "$test_dir/sqlite-storage-test"
"$test_dir/sqlite-storage-test" "$root_dir/server/migrations/001_initial.sql" "$test_dir"
