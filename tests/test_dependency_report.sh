#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/wena-dependency-report-XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
  $(sdl2-config --cflags) "$root_dir/tests/dependency_report_test.c" \
  "$root_dir/client/platform/dependencies.c" $(sdl2-config --libs) \
  -lsqlite3 -o "$test_dir/test"
"$test_dir/test"
