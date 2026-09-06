#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir="${TMPDIR:-/tmp}/wena-router-test-$$"
mkdir -p "$test_dir"
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
  "$root_dir/tests/router_test.c" "$root_dir/server/router.c" \
  "$root_dir/server/http.c" "$root_dir/server/security.c" \
  "$root_dir/imports/ui/page_contract.c" -o "$test_dir/router-test"
"$test_dir/router-test"
if grep -Eiq 'sqlite|models/|_init\(' "$root_dir/server/router.c"; then
  echo "router must return intents without storage/domain mutation" >&2
  exit 1
fi
