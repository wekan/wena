#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir="${TMPDIR:-/tmp}/wena-sdl-text-input-test-$$"
mkdir -p "$test_dir"
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror -DNK_INPUT_MAX=256 \
  $(sdl2-config --cflags) "$root_dir/tests/sdl_text_input_test.c" \
  "$root_dir/client/platform/sdl_nuklear.c" -o "$test_dir/test" \
  $(sdl2-config --libs) -lm
SDL_VIDEODRIVER=dummy "$test_dir/test"
