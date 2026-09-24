#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir="${TMPDIR:-/tmp}/wena-card-restore-persistence-test-$$"
mkdir -p "$test_dir"
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror "$root_dir/tests/card_restore_persistence_test.c" "$root_dir/client/features/card_mutation.c" "$root_dir/models/card_order.c" "$root_dir/models/card.c" "$root_dir/models/model.c" "$root_dir/server/sqlite_persistence.c" "$root_dir/server/mutations/list_archive.c" \
  "$root_dir/server/mutations/checklist_order.c" "$root_dir/models/checklist.c" "$root_dir/models/checklist_item.c" \
  "$root_dir/server/mutations/board_settings.c" \
  "$root_dir/server/mutations/checklist_batch.c" "$root_dir/models/checklist_item_titles.c" "$root_dir/models/text.c" \
  "$root_dir/server/mutations/labels.c" "$root_dir/models/label.c" "$root_dir/models/color.c" "$root_dir/server/sqlite_storage.c" "$root_dir/server/sha256.c" "$root_dir/server/region_response.c" -lsqlite3 -o "$test_dir/test"
"$test_dir/test" "$root_dir/server/migrations/001_initial.sql" "$test_dir"
