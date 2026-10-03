#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir="${TMPDIR:-/tmp}/wena-embedded-migration-test-$$";mkdir -p "$test_dir";trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
# A file carrying the migration and translation footers, appended by the same
# scripts the server runtime's executable will use (server/embedded_migration.c reads them).
printf 'wena footer fixture\n' > "$test_dir/footers"
python3 "$root_dir/scripts/embed_migrations.py" --executable "$test_dir/footers"
python3 "$root_dir/scripts/embed_i18n_catalog.py" --executable "$test_dir/footers"
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror "$root_dir/tests/embedded_migration_test.c" "$root_dir/server/embedded_migration.c" "$root_dir/server/sha256.c" -o "$test_dir/test"
"$test_dir/test" "$test_dir/footers" "$test_dir"
