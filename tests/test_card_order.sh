#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/wena-card-order-XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
 "$root_dir/tests/card_order_test.c" "$root_dir/models/card_order.c" \
 "$root_dir/models/card.c" "$root_dir/models/model.c" -o "$test_dir/test"
"$test_dir/test"
