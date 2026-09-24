#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/wena-collapse-prefs-XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
  -I"$root_dir/third_party/nuklear" \
  "$root_dir/tests/collapse_preferences_test.c" \
  "$root_dir/imports/preferences/collapse.c" "$root_dir/server/sha256.c" \
  "$root_dir/client/components/boards/board_layout.c" \
  "$root_dir/client/components/boards/board_header.c" \
  "$root_dir/client/components/sidebar/board_sidebar.c" "$root_dir/client/components/common/paginated_table.c" \
  "$root_dir/client/components/lists/list_header.c" "$root_dir/client/components/common/color_heading.c" \
  "$root_dir/client/components/cards/card_body.c" \
  "$root_dir/imports/ui/page_contract.c" "$root_dir/models/color.c" \
  "$root_dir/models/model.c" "$root_dir/models/board.c" \
  "$root_dir/models/swimlane.c" "$root_dir/models/list.c" \
  "$root_dir/models/card.c" -o "$test_dir/test" -lm
"$test_dir/test" "$test_dir/workspace.sqlite"
