#!/usr/bin/env sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir="${TMPDIR:-/tmp}/wena-locale-test-$$"
binary="$test_dir/locale-test"
mkdir -p "$test_dir"
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM

cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
  "$root_dir/tests/locale_test.c" \
  "$root_dir/imports/i18n/locale.c" \
  -o "$binary"
LC_ALL=C "$binary"
