#!/usr/bin/env sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir="${TMPDIR:-/tmp}/wena-card-description-sqlite-test-$$"
binary="$test_dir/card-description-sqlite-test"

mkdir -p "$test_dir"
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM

cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
  -I"$root_dir/tests/fakes" \
  -I"$root_dir/models" \
  "$root_dir/tests/card_description_sqlite_test.c" \
  "$root_dir/tests/fakes/nuklear.c" \
  "$root_dir/client/features/board.c" \
  "$root_dir/client/features/card_description.c" \
  "$root_dir/client/components/boards/board_layout.c" \
  "$root_dir/client/components/boards/board_header.c" \
  "$root_dir/client/components/sidebar/board_sidebar.c" "$root_dir/client/components/common/paginated_table.c" \
  "$root_dir/client/components/lists/list_header.c" "$root_dir/tests/fakes/color_heading.c" \
  "$root_dir/client/components/cards/card_body.c" \
  "$root_dir/client/components/cards/card_details_canvas.c" \
  "$root_dir/client/features/card_details.c" "$root_dir/client/components/forms/text_form.c" \
  "$root_dir/imports/ui/page_contract.c" \
  "$root_dir/models/model.c" \
  "$root_dir/models/board.c" \
  "$root_dir/models/swimlane.c" \
  "$root_dir/models/list.c" \
  "$root_dir/models/card.c" \
  "$root_dir/client/features/card_description_mutation.c" \
  "$root_dir/server/sqlite_persistence.c" "$root_dir/server/mutations/card_people.c" "$root_dir/server/mutations/selected_people.c" "$root_dir/server/card_people_store.c" "$root_dir/models/card_people.c" "$root_dir/models/card_order.c" "$root_dir/server/mutations/hierarchy_colors.c" "$root_dir/server/mutations/list_wip.c" "$root_dir/models/wip_limit.c" "$root_dir/server/list_state.c" "$root_dir/server/sqlite_storage.c" "$root_dir/server/mutations/list_archive.c" "$root_dir/server/mutations/card_archive.c" "$root_dir/server/mutations/swimlane_archive.c" \
  "$root_dir/server/mutations/checklist_order.c" "$root_dir/models/checklist.c" "$root_dir/models/checklist_item.c" \
  "$root_dir/server/mutations/board_settings.c" \
  "$root_dir/server/mutations/checklist_batch.c" "$root_dir/models/checklist_item_titles.c" "$root_dir/models/text.c" \
  "$root_dir/server/mutations/labels.c" "$root_dir/models/label.c" "$root_dir/models/color.c" \
  "$root_dir/server/sha256.c" \
  "$root_dir/server/region_response.c" \
  -lsqlite3 -o "$binary"
"$binary" "$root_dir/server/migrations/001_initial.sql" "$root_dir/tests/fixtures/card_description_schema_v2.sql" "$test_dir/database.sqlite"
