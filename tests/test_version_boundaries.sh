#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/wena-version-boundaries-XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
  "$root_dir/tests/version_boundaries_test.c" \
  "$root_dir/client/features/card_mutation.c" "$root_dir/models/card_order.c" \
  "$root_dir/client/features/card_description_mutation.c" \
  "$root_dir/client/features/hierarchy_mutation.c" \
  "$root_dir/client/features/hierarchy_move_mutation.c" \
  "$root_dir/client/features/checklist_mutation.c" "$root_dir/client/features/checklist_store.c" \
  "$root_dir/models/model.c" "$root_dir/models/card.c" "$root_dir/models/board.c" \
  "$root_dir/models/list.c" "$root_dir/models/swimlane.c" \
  "$root_dir/models/checklist.c" "$root_dir/models/checklist_item.c" \
  "$root_dir/models/checklist_item_titles.c" "$root_dir/models/text.c" \
  "$root_dir/models/label.c" "$root_dir/models/color.c" \
  "$root_dir/server/sqlite_board.c" "$root_dir/server/list_state.c" "$root_dir/server/sqlite_storage.c" "$root_dir/server/sqlite_persistence.c" "$root_dir/server/mutations/hierarchy_colors.c" "$root_dir/server/mutations/list_archive.c" \
  "$root_dir/server/mutations/checklist_order.c" \
  "$root_dir/server/mutations/board_settings.c" \
  "$root_dir/server/mutations/checklist_batch.c" "$root_dir/server/mutations/labels.c" \
  "$root_dir/server/sha256.c" "$root_dir/server/region_response.c" \
  -lsqlite3 -o "$test_dir/test"
"$test_dir/test" "$root_dir/server/migrations/001_initial.sql" \
  "$root_dir/server/migrations/002_card_descriptions.sql" \
  "$root_dir/server/migrations/003_checklists.sql" "$test_dir/database.sqlite"
