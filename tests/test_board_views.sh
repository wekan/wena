#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/wena-board-views-XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror -I"$root_dir/tests/fakes" \
  "$root_dir/tests/board_views_test.c" "$root_dir/client/components/boards/board_views.c" "$root_dir/models/charts.c" \
  "$root_dir/models/view_data.c" "$root_dir/client/components/common/wekan_look.c" "$root_dir/tests/fakes/nuklear.c" \
  "$root_dir/tests/fakes/nuklear_look.c" "$root_dir/imports/ui/page_contract.c" "$root_dir/models/color.c" -lm -o "$test_dir/test"
"$test_dir/test"
echo "Board views: WeKan's 35 views, their order, separators and charts, and a chart drawn passed"
