#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir="${TMPDIR:-/tmp}/wena-nuklear-checklist-order-$$"
mkdir -p "$test_dir"
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
  -I"$root_dir/third_party/nuklear" \
  "$root_dir/tests/nuklear_checklist_order_test.c" \
  "$root_dir/client/features/card_details.c" \
  "$root_dir/client/features/checklists.c" "$root_dir/client/components/common/card_section.c" "$root_dir/models/card_section.c" "$root_dir/client/features/checklist_store.c" \
  "$root_dir/client/components/cards/card_details_canvas.c" \
  "$root_dir/imports/ui/page_contract.c" "$root_dir/models/color.c" \
  "$root_dir/models/checklist.c" "$root_dir/models/checklist_item.c" "$root_dir/models/checklist_item_titles.c" "$root_dir/models/text.c" \
  "$root_dir/models/model.c" "$root_dir/models/card.c" \
  "$root_dir/models/board.c" "$root_dir/models/list.c" "$root_dir/models/swimlane.c" \
  -o "$test_dir/test" -lm
"$test_dir/test"
