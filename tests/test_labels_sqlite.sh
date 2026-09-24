#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir="${TMPDIR:-/tmp}/wena-labels-sqlite-test-$$"
mkdir -p "$test_dir"
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror ${WENA_TEST_CFLAGS:-} \
  -I"$root_dir/tests/fakes" \
  "$root_dir/tests/labels_sqlite_test.c" "$root_dir/tests/fakes/nuklear.c" \
  "$root_dir/tests/fakes/labels_component.c" \
  "$root_dir/client/features/labels/panel.c" "$root_dir/client/components/forms/color_input.c" "$root_dir/client/features/labels/store.c" \
  "$root_dir/client/features/labels/mutation.c" \
  "$root_dir/client/features/card_details.c" "$root_dir/client/components/forms/text_form.c" \
  "$root_dir/client/components/cards/card_details_canvas.c" \
  "$root_dir/imports/ui/page_contract.c" \
  "$root_dir/models/label.c" "$root_dir/models/color.c" "$root_dir/models/text.c" \
  "$root_dir/models/checklist_item_titles.c" \
  "$root_dir/models/model.c" "$root_dir/models/card.c" \
  "$root_dir/server/sqlite_persistence.c" "$root_dir/models/card_order.c" "$root_dir/server/mutations/hierarchy_colors.c" "$root_dir/server/mutations/list_wip.c" "$root_dir/models/wip_limit.c" "$root_dir/server/list_state.c" "$root_dir/server/sqlite_storage.c" "$root_dir/server/mutations/list_archive.c" "$root_dir/server/mutations/card_archive.c" "$root_dir/server/mutations/swimlane_archive.c" \
  "$root_dir/server/mutations/checklist_order.c" "$root_dir/models/checklist.c" "$root_dir/models/checklist_item.c" \
  "$root_dir/server/mutations/board_settings.c" "$root_dir/server/mutations/labels.c" \
  "$root_dir/server/mutations/checklist_batch.c" \
  "$root_dir/server/sha256.c" "$root_dir/server/region_response.c" \
  -lsqlite3 -o "$test_dir/test"
"$test_dir/test" "$root_dir/server/migrations/001_initial.sql" \
  "$root_dir/server/migrations/002_card_descriptions.sql" \
  "$root_dir/server/migrations/005_labels.sql" "$test_dir/labels.sqlite" \
  "$root_dir/server/migrations/010_list_archive_state.sql" \
  "$root_dir/server/migrations/013_swimlane_archive_state.sql"
