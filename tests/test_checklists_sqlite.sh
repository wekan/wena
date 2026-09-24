#!/usr/bin/env sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir="${TMPDIR:-/tmp}/wena-checklists-sqlite-test-$$"
binary="$test_dir/checklists-sqlite-test"

mkdir -p "$test_dir"
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM

cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
  -I"$root_dir/tests/fakes" \
  -I"$root_dir/models" \
  "$root_dir/tests/checklists_sqlite_test.c" \
  "$root_dir/tests/fakes/nuklear.c" \
  "$root_dir/client/features/board.c" \
  "$root_dir/client/features/card_destination.c" \
  "$root_dir/client/features/directory_picker.c" \
  "$root_dir/models/directory.c" \
  "$root_dir/client/components/forms/position_input.c" "$root_dir/client/features/checklists.c" "$root_dir/client/features/checklists/entry_form.c" "$root_dir/client/components/common/card_section.c" "$root_dir/models/card_section.c" "$root_dir/client/features/checklist_store.c" \
  "$root_dir/client/components/boards/board_layout.c" \
  "$root_dir/client/components/boards/board_header.c" \
  "$root_dir/client/components/sidebar/board_sidebar.c" "$root_dir/client/components/common/paginated_table.c" \
  "$root_dir/client/components/lists/list_header.c" \
  "$root_dir/client/components/cards/card_body.c" \
  "$root_dir/client/components/cards/card_details_canvas.c" \
  "$root_dir/client/features/card_details.c" "$root_dir/client/components/forms/text_form.c" \
  "$root_dir/imports/ui/page_contract.c" \
  "$root_dir/models/checklist.c" \
  "$root_dir/models/checklist_item.c" \
  "$root_dir/models/checklist_item_titles.c" "$root_dir/models/text.c" \
  "$root_dir/models/model.c" \
  "$root_dir/models/board.c" \
  "$root_dir/models/swimlane.c" \
  "$root_dir/models/list.c" \
  "$root_dir/models/card.c" \
  "$root_dir/client/features/checklist_mutation.c" \
  "$root_dir/server/sqlite_persistence.c" "$root_dir/server/mutations/list_archive.c" \
  "$root_dir/server/mutations/checklist_order.c" \
  "$root_dir/server/mutations/board_settings.c" \
  "$root_dir/server/mutations/checklist_batch.c" \
  "$root_dir/server/mutations/labels.c" "$root_dir/models/label.c" "$root_dir/models/color.c" "$root_dir/server/sha256.c" "$root_dir/server/region_response.c" \
  -lsqlite3 -o "$binary"
"$binary" "$root_dir/server/migrations/001_initial.sql" "$root_dir/server/migrations/002_card_descriptions.sql" "$root_dir/tests/fixtures/checklist_schema_v3.sql" "$test_dir/checklist.sqlite"
