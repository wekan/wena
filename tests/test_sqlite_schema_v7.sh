#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/wena-schema-v7-XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
python3 - "$root_dir" "$test_dir" <<'PY'
from pathlib import Path
import sys
sys.path.insert(0, str(Path(sys.argv[1]) / 'scripts'))
from verify_migrations import verify
lock, bundle = verify(Path(sys.argv[1]))
for version in range(1, 8):
    (Path(sys.argv[2]) / ('v%d.sql' % version)).write_bytes(bundle[:lock['migrations'][version-1]['bundle_size']])
PY
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
 "$root_dir/tests/sqlite_schema_v7_test.c" "$root_dir/server/sqlite_storage.c" \
 "$root_dir/server/sha256.c" -lsqlite3 -o "$test_dir/test"
"$test_dir/test" "$test_dir/v1.sql" "$test_dir/v2.sql" "$test_dir/v3.sql" "$test_dir/v4.sql" "$test_dir/v5.sql" "$test_dir/v6.sql" "$test_dir/v7.sql" "$test_dir"
