#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir="${TMPDIR:-/tmp}/wena-platform-security-test-$$"
mkdir -p "$test_dir"
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
  "$root_dir/tests/platform_security_test.c" \
  "$root_dir/server/os_entropy.c" "$root_dir/server/response_policy.c" \
  "$root_dir/server/security.c" "$root_dir/server/settings.c" \
  -o "$test_dir/platform-security-test"
"$test_dir/platform-security-test"
