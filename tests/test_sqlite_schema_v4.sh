#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/wena-schema-v4-XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
python3 - "$root_dir" "$test_dir" <<'PY'
from pathlib import Path
import ast
import re
import sys
root = Path(sys.argv[1])
sys.path.insert(0, str(root / 'scripts'))
from verify_migrations import verify
lock, bundle = verify(root)
for version in range(1, 5):
    (Path(sys.argv[2]) / ('v%d.sql' % version)).write_bytes(bundle[:lock['migrations'][version-1]['bundle_size']])
loader = root / 'client/features/checklist_mutation.c'
if loader.exists():
    literal = r'"(?:\\.|[^"\\])*"'
    source = ''.join(ast.literal_eval(value) for value in re.findall(literal, loader.read_text()))
    test = (root / 'tests/sqlite_schema_v4_test.c').read_text()
    query = ast.literal_eval(re.search(r'static const char item_query\[\] = (' + literal + r');', test).group(1))
    assert query in source, 'Update query-work fixture to match the actual selected-card loader query'
PY
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
 "$root_dir/tests/sqlite_schema_v4_test.c" "$root_dir/server/sqlite_storage.c" \
 "$root_dir/server/sqlite_backup.c" "$root_dir/server/sqlite_restore.c" \
 "$root_dir/server/sha256.c" -lsqlite3 -o "$test_dir/test"
"$test_dir/test" "$test_dir/v1.sql" "$test_dir/v2.sql" "$test_dir/v3.sql" "$test_dir/v4.sql" "$test_dir"
