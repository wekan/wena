#!/usr/bin/env sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir="${TMPDIR:-/tmp}/wena-nuklear-test-$$"
binary="$test_dir/nuklear-test"

if ! command -v sdl2-config >/dev/null 2>&1; then
  echo "SDL2 development files not found (expected sdl2-config)" >&2
  exit 1
fi

mkdir -p "$test_dir"
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM

cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
  -I"$root_dir/third_party/nuklear" \
  $(sdl2-config --cflags) \
  "$root_dir/tests/nuklear_integration_test.c" \
  "$root_dir/client/platform/sdl_nuklear.c" \
  "$root_dir/client/features/board.c" \
  "$root_dir/client/components/boards/board_layout.c" \
  "$root_dir/models/model.c" \
  "$root_dir/models/board.c" \
  -o "$binary" \
  $(sdl2-config --libs) -lm
"$binary"
