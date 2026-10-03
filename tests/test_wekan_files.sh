#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/wena-wekan-files-XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
  "$root_dir/tests/wekan_files_test.c" "$root_dir/client/platform/wekan_files.c" \
  "$root_dir/client/platform/debug_log.c" "$root_dir/client/platform/files.c" \
  -o "$test_dir/test"
"$test_dir/test" "$test_dir"
