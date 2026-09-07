#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir="${TMPDIR:-/tmp}/wena-card-editor-sqlite-$$"
mkdir -p "$test_dir"
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
  -I"$root_dir/tests/fakes" \
  "$root_dir/tests/card_editor_sqlite_test.c" \
  "$root_dir/tests/fakes/nuklear.c" \
  "$root_dir/client/features/card_details.c" \
  "$root_dir/client/components/cards/card_details_canvas.c" \
  "$root_dir/client/features/card_mutation.c" \
  "$root_dir/imports/ui/page_contract.c" \
  "$root_dir/models/card.c" "$root_dir/models/model.c" \
  "$root_dir/server/sqlite_persistence.c" \
  "$root_dir/server/sqlite_storage.c" "$root_dir/server/sha256.c" \
  "$root_dir/server/region_response.c" -lsqlite3 -o "$test_dir/test"
"$test_dir/test" "$root_dir/server/migrations/001_initial.sql" "$test_dir"
