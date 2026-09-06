#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir="${TMPDIR:-/tmp}/wena-server-settings-test-$$"
mkdir -p "$test_dir"
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
  "$root_dir/tests/server_settings_test.c" "$root_dir/server/settings.c" \
  "$root_dir/client/features/server_settings.c" \
  -o "$test_dir/server-settings-test"
"$test_dir/server-settings-test"
