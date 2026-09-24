#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/wena-reorder-drag-XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror -isystem "$root_dir/third_party/nuklear" \
 "$root_dir/tests/nuklear_reorder_drag_test.c" "$root_dir/client/components/common/reorder_drag.c" \
 "$root_dir/models/model.c" -lm -o "$test_dir/test"
"$test_dir/test"
