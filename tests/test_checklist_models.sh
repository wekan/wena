#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir="${TMPDIR:-/tmp}/wena-checklist-models-$$"
mkdir -p "$test_dir"
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
 "$root_dir/tests/checklist_model_test.c" "$root_dir/models/model.c" "$root_dir/models/card.c" \
 "$root_dir/models/checklist.c" "$root_dir/models/checklist_item.c" -o "$test_dir/test"
"$test_dir/test"
