#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir="${TMPDIR:-/tmp}/wena-html4-render-test-$$"
mkdir -p "$test_dir"
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
  "$root_dir/tests/legacy_html4_render_test.c" \
  "$root_dir/server/settings.c" "$root_dir/server/root_url.c" \
  "$root_dir/server/legacy_html4.c" "$root_dir/imports/ui/page_contract.c" \
  -o "$test_dir/html4-render-test"
"$test_dir/html4-render-test"
