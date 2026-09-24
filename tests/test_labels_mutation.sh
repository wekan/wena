#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir="${TMPDIR:-/tmp}/wena-labels-mutation-$$"
mkdir -p "$test_dir"
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
labels_schema=${WENA_LABEL_SCHEMA:-$root_dir/server/migrations/005_labels.sql}
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror ${WENA_TEST_CFLAGS:-} \
 "$root_dir/tests/labels_mutation_test.c" "$root_dir/client/features/labels/mutation.c" "$root_dir/client/features/labels/store.c" \
 "$root_dir/models/model.c" "$root_dir/models/label.c" "$root_dir/models/color.c" \
 "$root_dir/models/checklist_item_titles.c" "$root_dir/models/text.c" \
 "$root_dir/server/sqlite_persistence.c" "$root_dir/server/mutations/hierarchy_colors.c" "$root_dir/server/mutations/list_wip.c" "$root_dir/models/wip_limit.c" "$root_dir/server/list_state.c" "$root_dir/server/sqlite_storage.c" "$root_dir/server/mutations/list_archive.c" \
  "$root_dir/server/mutations/checklist_order.c" "$root_dir/models/checklist.c" "$root_dir/models/checklist_item.c" \
  "$root_dir/server/mutations/board_settings.c" "$root_dir/server/mutations/labels.c" "$root_dir/server/mutations/checklist_batch.c" \
 "$root_dir/server/sha256.c" "$root_dir/server/region_response.c" \
 -lsqlite3 -o "$test_dir/test"
"$test_dir/test" "$root_dir/server/migrations/001_initial.sql" "$root_dir/server/migrations/002_card_descriptions.sql" "$labels_schema" "$test_dir/labels.sqlite"
