#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir="${TMPDIR:-/tmp}/wena-executable-path-test-$$"
mkdir -p "$test_dir"
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror "$root_dir/tests/executable_path_test.c" "$root_dir/server/executable_path.c" -o "$test_dir/test"
"$test_dir/test"
