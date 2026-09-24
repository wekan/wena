#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
export WENA_TRANSFER_TEST_DEFINE=-DWENA_TEST_CROSS_BOARD
sh "$root_dir/tests/test_checklist_move.sh"
sh "$root_dir/tests/test_checklist_item_move.sh"
