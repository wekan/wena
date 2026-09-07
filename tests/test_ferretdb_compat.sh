#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir="${TMPDIR:-/tmp}/wena-ferretdb-compat-test-$$"
mkdir -p "$test_dir"
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror "$root_dir/tests/ferretdb_compat_test.c" "$root_dir/server/ferretdb_compat.c" -lsqlite3 -o "$test_dir/test"
real_db="$root_dir/../FerretDB/state-debug-speed/wekan.sqlite"
if test -f "$real_db"; then "$test_dir/test" "$test_dir" "$real_db"; else "$test_dir/test" "$test_dir"; fi
