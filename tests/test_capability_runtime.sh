#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/wena-capability-runtime-XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
  "$root_dir/tests/capability_dump.c" "$root_dir/server/capability.c" \
  -o "$test_dir/capability-dump"
"$test_dir/capability-dump" > "$test_dir/capability.js"
node "$root_dir/tests/capability_runtime_test.js" "$test_dir/capability.js"
