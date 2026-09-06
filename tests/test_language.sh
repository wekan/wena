#!/usr/bin/env sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir="${TMPDIR:-/tmp}/wena-language-test-$$"
binary="$test_dir/language-test"
settings="$test_dir/language.txt"
mkdir -p "$test_dir"
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM

cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
  "$root_dir/tests/language_test.c" \
  "$root_dir/imports/i18n/language.c" \
  "$root_dir/imports/i18n/locale.c" \
  -o "$binary"
LC_ALL=C "$binary" "$settings"
