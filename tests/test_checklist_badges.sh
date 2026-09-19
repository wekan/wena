#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/wena-checklist-badges-XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror ${WENA_TEST_CFLAGS:-} \
  -I"$root_dir/tests/fakes" \
  "$root_dir/tests/checklist_badges_test.c" "$root_dir/tests/fakes/nuklear.c" \
  "$root_dir/client/features/checklists/badges.c" "$root_dir/client/features/checklists/summary.c" \
  "$root_dir/models/model.c" "$root_dir/models/card.c" \
  "$root_dir/models/checklist.c" "$root_dir/models/checklist_item.c" \
  "$root_dir/imports/ui/page_contract.c" "$root_dir/models/color.c" \
  -lsqlite3 -o "$test_dir/test"
"$test_dir/test"
