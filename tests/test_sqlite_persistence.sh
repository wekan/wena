#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir="${TMPDIR:-/tmp}/wena-sqlite-persistence-test-$$"
mkdir -p "$test_dir"
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror "$root_dir/tests/sqlite_persistence_test.c" "$root_dir/server/sqlite_persistence.c" "$root_dir/server/sqlite_storage.c" "$root_dir/server/sha256.c" "$root_dir/server/region_response.c" -lsqlite3 -o "$test_dir/test"
"$test_dir/test" "$root_dir/server/migrations/001_initial.sql" "$test_dir"
