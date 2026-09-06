#!/usr/bin/env sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir="${TMPDIR:-/tmp}/wena-model-test-$$"

mkdir -p "$test_dir"
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM

for suite in model_test model_validation_test; do
  binary="$test_dir/$suite"
  cc \
    -std=c89 \
    -pedantic-errors \
    -Wall \
    -Wextra \
    -Werror \
    -I"$root_dir/models" \
    "$root_dir/tests/$suite.c" \
    "$root_dir/models/model.c" \
    "$root_dir/models/board.c" \
    "$root_dir/models/swimlane.c" \
    "$root_dir/models/list.c" \
    "$root_dir/models/card.c" \
    -o "$binary"
  "$binary"
done
