#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir="${TMPDIR:-/tmp}/wena-embedded-migration-test-$$";mkdir -p "$test_dir";trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
"$root_dir/build.sh" build host
target=$(python3 -c "import sys;sys.path.insert(0,'$root_dir/scripts');import wena;print(wena.host_target())")
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror "$root_dir/tests/embedded_migration_test.c" "$root_dir/server/embedded_migration.c" "$root_dir/server/sha256.c" -o "$test_dir/test"
"$test_dir/test" "$root_dir/dist/$target/wena" "$test_dir"
