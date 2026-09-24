#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/wena-contents-XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
python3 - "$root_dir" "$test_dir" <<'PY'
from pathlib import Path
import sys
sys.path.insert(0, str(Path(sys.argv[1]) / 'scripts'))
from verify_migrations import verify
lock, bundle = verify(Path(sys.argv[1]))
(Path(sys.argv[2]) / 'schema.sql').write_bytes(bundle)
PY
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
 -isystem "$root_dir/third_party/nuklear" \
 "$root_dir/tests/nuklear_checklist_contents_test.c" \
 "$root_dir/client/components/common/reorder_drag.c" "$root_dir/client/features/checklists/drag.c" \
    "$root_dir/client/features/checklists/inline_edit.c" \
    "$root_dir/client/components/cards/checklist_contents.c" "$root_dir/client/components/common/card_section.c" "$root_dir/models/card_section.c" \
 "$root_dir/client/features/checklists/summary.c" \
 "$root_dir/models/model.c" "$root_dir/models/card.c" \
 "$root_dir/models/checklist.c" "$root_dir/models/checklist_item.c" \
 "$root_dir/imports/preferences/sections.c" "$root_dir/client/features/card_destination.c" \
  "$root_dir/client/features/directory_picker.c" \
  "$root_dir/client/components/common/paginated_table.c" \
  "$root_dir/models/directory.c" \
  "$root_dir/client/components/forms/position_input.c" "$root_dir/client/features/checklists.c" "$root_dir/client/features/checklists/entry_form.c" \
 "$root_dir/client/features/card_details.c" "$root_dir/client/components/forms/text_form.c" "$root_dir/client/components/cards/card_details_canvas.c" \
 "$root_dir/models/board.c" "$root_dir/models/list.c" "$root_dir/models/swimlane.c" \
 "$root_dir/client/features/checklist_mutation.c" "$root_dir/client/features/checklist_store.c" \
 "$root_dir/imports/ui/page_contract.c" "$root_dir/models/label.c" "$root_dir/models/color.c" "$root_dir/models/text.c" "$root_dir/models/checklist_item_titles.c" \
 "$root_dir/server/sqlite_persistence.c" "$root_dir/server/mutations/hierarchy_colors.c" "$root_dir/server/mutations/list_wip.c" "$root_dir/models/wip_limit.c" "$root_dir/server/list_state.c" "$root_dir/server/sqlite_storage.c" "$root_dir/server/mutations/list_archive.c" "$root_dir/server/mutations/board_settings.c" \
 "$root_dir/server/mutations/checklist_order.c" "$root_dir/server/mutations/labels.c" "$root_dir/server/mutations/checklist_batch.c" \
 "$root_dir/server/sha256.c" "$root_dir/server/region_response.c" \
 -lsqlite3 -lm -o "$test_dir/test"
"$test_dir/test" "$test_dir/schema.sql"
