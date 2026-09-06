#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir="${TMPDIR:-/tmp}/wena-capability-test-$$"
mkdir -p "$test_dir"
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
  "$root_dir/tests/capability_dump.c" "$root_dir/server/capability.c" \
  -o "$test_dir/capability-dump"
"$test_dir/capability-dump" > "$test_dir/capability.js"
python3 "$root_dir/tests/test_capability.py" "$test_dir/capability.js"
