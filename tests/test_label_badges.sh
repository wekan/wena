#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/wena-label-badges-XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror ${WENA_TEST_CFLAGS:-} \
  -I"$root_dir/tests/fakes" \
  "$root_dir/tests/label_badges_test.c" "$root_dir/tests/fakes/nuklear.c" \
  "$root_dir/tests/fakes/labels_component.c" \
  "$root_dir/client/features/labels/badges.c" "$root_dir/client/features/labels/store.c" \
  "$root_dir/models/label.c" "$root_dir/models/color.c" \
  "$root_dir/models/model.c" "$root_dir/models/card.c" \
  -o "$test_dir/test"
"$test_dir/test"
