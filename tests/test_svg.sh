#!/usr/bin/env sh
# SPDX-License-Identifier: MIT
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/wena-svg-XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
python3 "$root_dir/scripts/compile_svg.py" --check
python3 "$root_dir/tests/test_svg.py"
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
  -isystem "$root_dir/third_party/nuklear" \
  "$root_dir/tests/svg_test.c" "$root_dir/client/platform/svg.c" \
  "$root_dir/client/platform/theme.c" "$root_dir/client/components/boards/board_header.c" \
  "$root_dir/models/model.c" "$root_dir/models/board.c" \
  "$root_dir/imports/ui/page_contract.c" "$root_dir/models/color.c" -o "$test_dir/test" -lm
"$test_dir/test"
