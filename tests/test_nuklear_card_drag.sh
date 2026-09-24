#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir="${TMPDIR:-/tmp}/wena-card-drag-$$"
mkdir -p "$test_dir"
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -isystem "$root_dir/third_party/nuklear" -std=c89 -pedantic-errors -Wall -Wextra -Werror \
 "$root_dir/tests/nuklear_card_drag_test.c" "$root_dir/client/features/card_drag.c" "$root_dir/client/features/boards/reload.c" \
 "$root_dir/models/card_order.c" "$root_dir/client/components/common/reorder_drag.c" "$root_dir/imports/ui/page_contract.c" "$root_dir/client/features/card_mutation.c" \
 "$root_dir/models/model.c" "$root_dir/models/board.c" "$root_dir/models/list.c" "$root_dir/models/swimlane.c" "$root_dir/models/card.c" \
 "$root_dir/server/sqlite_board.c" "$root_dir/server/list_state.c" "$root_dir/server/sqlite_persistence.c" "$root_dir/server/mutations/hierarchy_colors.c" "$root_dir/server/mutations/list_archive.c" \
  "$root_dir/server/mutations/checklist_order.c" "$root_dir/models/checklist.c" "$root_dir/models/checklist_item.c" \
  "$root_dir/server/mutations/board_settings.c" \
  "$root_dir/server/mutations/checklist_batch.c" "$root_dir/models/checklist_item_titles.c" "$root_dir/models/text.c" \
  "$root_dir/server/mutations/labels.c" "$root_dir/models/label.c" "$root_dir/models/color.c" "$root_dir/server/sqlite_storage.c" "$root_dir/server/sha256.c" "$root_dir/server/region_response.c" \
 -lsqlite3 -lm -o "$test_dir/test"
"$test_dir/test" "$root_dir/server/migrations/001_initial.sql"
