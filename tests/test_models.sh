#!/usr/bin/env sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir="${TMPDIR:-/tmp}/wena-model-test-$$"
binary="$test_dir/model-test"

mkdir -p "$test_dir"
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM

cc \
  -std=c89 \
  -pedantic-errors \
  -Wall \
  -Wextra \
  -Werror \
  -I"$root_dir/models" \
  "$root_dir/tests/model_test.c" \
  "$root_dir/models/model.c" \
  "$root_dir/models/board.c" \
  "$root_dir/models/swimlane.c" \
  "$root_dir/models/list.c" \
  "$root_dir/models/card.c" \
  -o "$binary"
"$binary"
