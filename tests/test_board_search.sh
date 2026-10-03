#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/wena-board-search-XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
  "$root_dir/tests/board_search_test.c" "$root_dir/server/board_search.c" "$root_dir/models/model.c" \
  "$root_dir/server/sqlite_storage.c" "$root_dir/server/sha256.c" -lsqlite3 -o "$test_dir/test"
"$test_dir/test"
echo "Board search: titles and descriptions, case, trimming, LIKE characters, other boards and capacity passed"
