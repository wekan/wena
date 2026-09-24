#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir="${TMPDIR:-/tmp}/wena-checklist-delete-$$"
mkdir -p "$test_dir"
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
python3 - "$root_dir/tests/fixtures/checklist_schema_v3.sql" <<'PYCODE'
import hashlib, pathlib, sys
assert hashlib.sha256(pathlib.Path(sys.argv[1]).read_bytes()).hexdigest() == "944b67e3548c07694a86f630fa3acd0d61bd298e8df5c648d9bb2d7da6fc886c"
PYCODE
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
 "$root_dir/tests/checklist_delete_test.c" "$root_dir/client/features/checklist_mutation.c" "$root_dir/client/features/checklist_store.c" \
 "$root_dir/models/model.c" "$root_dir/models/card.c" "$root_dir/models/checklist.c" "$root_dir/models/checklist_item.c" \
 "$root_dir/server/sqlite_persistence.c" "$root_dir/server/mutations/hierarchy_colors.c" "$root_dir/server/mutations/list_wip.c" "$root_dir/models/wip_limit.c" "$root_dir/server/list_state.c" "$root_dir/server/sqlite_storage.c" "$root_dir/server/mutations/list_archive.c" "$root_dir/server/mutations/card_archive.c" "$root_dir/server/mutations/swimlane_archive.c" \
  "$root_dir/server/mutations/checklist_order.c" \
  "$root_dir/server/mutations/board_settings.c" \
  "$root_dir/server/mutations/checklist_batch.c" "$root_dir/models/checklist_item_titles.c" "$root_dir/models/text.c" \
  "$root_dir/server/mutations/labels.c" "$root_dir/models/label.c" "$root_dir/models/color.c" "$root_dir/server/sha256.c" "$root_dir/server/region_response.c" \
 -lsqlite3 -o "$test_dir/test"
"$test_dir/test" "$root_dir/server/migrations/001_initial.sql" "$root_dir/tests/fixtures/card_description_schema_v2.sql" "$root_dir/tests/fixtures/checklist_schema_v3.sql" "$test_dir/checklist.sqlite"
