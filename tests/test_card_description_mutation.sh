#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir="${TMPDIR:-/tmp}/wena-card-description-mutation-$$"
mkdir -p "$test_dir"
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
python3 - "$root_dir/tests/fixtures/card_description_schema_v2.sql" <<'PYCODE'
import hashlib, pathlib, sys
fixture = pathlib.Path(sys.argv[1]).read_bytes()
assert hashlib.sha256(fixture).hexdigest() == "429503c784a355f492d4ca6e65428a5e38375ec9d04fc63d264a9ffe1cf6ad83"
PYCODE
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
 "$root_dir/tests/card_description_mutation_test.c" "$root_dir/client/features/card_description_mutation.c" \
 "$root_dir/models/model.c" "$root_dir/server/sqlite_persistence.c" "$root_dir/server/mutations/card_people.c" "$root_dir/server/mutations/selected_people.c" "$root_dir/server/card_people_store.c" "$root_dir/models/card_people.c" "$root_dir/models/card_order.c" "$root_dir/server/mutations/hierarchy_colors.c" "$root_dir/server/mutations/list_wip.c" "$root_dir/models/wip_limit.c" "$root_dir/server/list_state.c" "$root_dir/server/sqlite_storage.c" "$root_dir/server/mutations/list_archive.c" "$root_dir/server/mutations/card_archive.c" "$root_dir/server/mutations/swimlane_archive.c" \
  "$root_dir/server/mutations/checklist_order.c" "$root_dir/models/checklist.c" "$root_dir/models/checklist_item.c" \
  "$root_dir/server/mutations/board_settings.c" \
  "$root_dir/server/mutations/checklist_batch.c" "$root_dir/models/checklist_item_titles.c" "$root_dir/models/text.c" \
  "$root_dir/server/mutations/labels.c" "$root_dir/models/label.c" "$root_dir/models/color.c" "$root_dir/server/sha256.c" "$root_dir/server/region_response.c" \
 -lsqlite3 -o "$test_dir/test"
"$test_dir/test" "$root_dir/server/migrations/001_initial.sql" "$root_dir/tests/fixtures/card_description_schema_v2.sql" "$test_dir/description.sqlite"
