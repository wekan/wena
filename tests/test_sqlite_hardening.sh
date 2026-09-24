#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/wena-sqlite-hardening-XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
  "$root_dir/tests/sqlite_hardening_test.c" "$root_dir/server/sqlite_storage.c" \
  "$root_dir/server/sqlite_board.c" "$root_dir/server/list_state.c" "$root_dir/server/sqlite_backup.c" \
  "$root_dir/server/sqlite_restore.c" "$root_dir/server/sha256.c" \
  "$root_dir/models/model.c" "$root_dir/models/board.c" \
  "$root_dir/models/swimlane.c" "$root_dir/models/list.c" "$root_dir/models/card.c" \
  -lsqlite3 -o "$test_dir/test"
"$test_dir/test" "$root_dir/server/migrations/001_initial.sql" "$test_dir"
