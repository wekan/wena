#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/wena-wekan-views-XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
  "$root_dir/tests/wekan_views_test.c" "$root_dir/server/wekan_views.c" "$root_dir/server/wekan_sync.c" \
  "$root_dir/server/ferretdb_sqlite.c" "$root_dir/models/view_data.c" "$root_dir/models/model.c" "$root_dir/models/board.c" \
  "$root_dir/models/list.c" "$root_dir/models/swimlane.c" "$root_dir/models/card.c" "$root_dir/models/color.c" \
  "$root_dir/models/wip_limit.c" "$root_dir/server/sqlite_board.c" "$root_dir/server/list_state.c" \
  "$root_dir/server/sqlite_storage.c" "$root_dir/server/sha256.c" -lsqlite3 -o "$test_dir/test"
"$test_dir/test" "$test_dir"
