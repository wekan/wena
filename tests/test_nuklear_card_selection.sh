#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/wena-nuklear-card-selection-XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror -I"$root_dir/third_party/nuklear" \
 "$root_dir/tests/nuklear_card_selection_test.c" "$root_dir/client/features/card_selection_panel.c" \
 "$root_dir/client/components/common/paginated_table.c" "$root_dir/client/components/forms/text_form.c" \
 "$root_dir/models/card_selection.c" "$root_dir/models/card.c" "$root_dir/models/model.c" \
 "$root_dir/models/color.c" "$root_dir/models/board.c" "$root_dir/imports/ui/page_contract.c" -lm -o "$test_dir/test"
"$test_dir/test"
