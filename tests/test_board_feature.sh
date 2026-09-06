#!/usr/bin/env sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir="${TMPDIR:-/tmp}/wena-board-feature-test-$$"
binary="$test_dir/board-feature-test"

mkdir -p "$test_dir"
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM

cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
  -I"$root_dir/tests/fakes" \
  -I"$root_dir/models" \
  "$root_dir/tests/board_feature_test.c" \
  "$root_dir/client/features/board.c" \
  "$root_dir/client/components/boards/board_layout.c" \
  "$root_dir/client/components/boards/board_header.c" \
  "$root_dir/client/components/sidebar/board_sidebar.c" \
  "$root_dir/client/components/lists/list_header.c" \
  "$root_dir/client/components/cards/card_body.c" \
  "$root_dir/client/components/cards/card_details_canvas.c" \
  "$root_dir/client/features/card_details.c" \
  "$root_dir/models/model.c" \
  "$root_dir/models/board.c" \
  "$root_dir/models/swimlane.c" \
  "$root_dir/models/list.c" \
  "$root_dir/models/card.c" \
  -o "$binary"
"$binary"
