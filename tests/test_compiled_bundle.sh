#!/usr/bin/env sh
# The desktop opens workspaces with the compiled migration bundle, never with
# bytes read back from its own executable file.
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir="${TMPDIR:-/tmp}/wena-compiled-bundle-test-$$"
mkdir -p "$test_dir"
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
expected=$(python3 -c 'import json,sys; m=json.load(open(sys.argv[1]))["migrations"][-1]; print(m["bundle_size"], m["bundle_sha256"])' \
  "$root_dir/config/migrations-lock.json")
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
  "$root_dir/tests/compiled_bundle_test.c" "$root_dir/server/sqlite_storage.c" \
  "$root_dir/server/sha256.c" -lsqlite3 -o "$test_dir/compiled-bundle-test"
# shellcheck disable=SC2086
"$test_dir/compiled-bundle-test" $expected
# Negative: the desktop reads no migration or catalog from its own file.
desktop="$root_dir/client/desktop.c"
for forbidden in wena_embedded_migration_load wena_i18n_catalog_open; do
  if grep -q "$forbidden" "$desktop"; then
    echo "client/desktop.c still calls $forbidden" >&2
    exit 1
  fi
done
grep -q wena_sqlite_compiled_bundle "$desktop"
# Nor does its build append anything to the executable.
if grep -q 'embed_migrations.py\|embed_i18n_catalog.py' "$root_dir/scripts/build_desktop.sh"; then
  echo "scripts/build_desktop.sh still appends footers to the desktop" >&2
  exit 1
fi
