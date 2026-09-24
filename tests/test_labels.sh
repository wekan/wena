#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir="${TMPDIR:-/tmp}/wena-labels-ui-test-$$"
mkdir -p "$test_dir"
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror ${WENA_TEST_CFLAGS:-} \
  -I"$root_dir/tests/fakes" \
  "$root_dir/tests/labels_test.c" "$root_dir/tests/fakes/nuklear.c" \
  "$root_dir/tests/fakes/labels_component.c" \
  "$root_dir/client/features/labels/panel.c" "$root_dir/client/components/forms/color_input.c" "$root_dir/client/features/labels/store.c" \
  "$root_dir/client/features/card_details.c" "$root_dir/client/components/forms/text_form.c" \
  "$root_dir/client/components/cards/card_details_canvas.c" \
  "$root_dir/imports/ui/page_contract.c" \
  "$root_dir/models/label.c" "$root_dir/models/color.c" "$root_dir/models/text.c" \
  "$root_dir/models/model.c" "$root_dir/models/card.c" \
  -o "$test_dir/test"
"$test_dir/test"
