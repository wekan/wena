#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir="${TMPDIR:-/tmp}/wena-http-test-$$"
mkdir -p "$test_dir"
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
  "$root_dir/tests/http_server_test.c" "$root_dir/server/http.c" \
  "$root_dir/server/http_listener.c" "$root_dir/server/settings.c" \
  -o "$test_dir/http-server-test"
"$test_dir/http-server-test"
