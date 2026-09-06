#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir="${TMPDIR:-/tmp}/wena-http-serving-test-$$"
mkdir -p "$test_dir"
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
  "$root_dir/tests/http_serving_test.c" \
  "$root_dir/server/http_listener.c" "$root_dir/server/http.c" \
  "$root_dir/server/capability.c" \
  "$root_dir/server/settings.c" "$root_dir/server/response_policy.c" \
  "$root_dir/server/router.c" "$root_dir/server/security.c" \
  "$root_dir/server/legacy_html4.c" "$root_dir/server/root_url.c" \
  "$root_dir/imports/ui/page_contract.c" -o "$test_dir/http-serving-test"
"$test_dir/http-serving-test"
grep -q 'SO_RCVTIMEO' "$root_dir/server/http_listener.c"
grep -q 'SO_SNDTIMEO' "$root_dir/server/http_listener.c"
