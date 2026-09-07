#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/wena-schema-v5-XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
python3 - "$root_dir" "$test_dir" <<'PY'
from pathlib import Path
import sys
import ast
import re
sys.path.insert(0, str(Path(sys.argv[1]) / 'scripts'))
from verify_migrations import verify
lock, bundle = verify(Path(sys.argv[1]))
for version in range(1, 6):
    (Path(sys.argv[2]) / ('v%d.sql' % version)).write_bytes(bundle[:lock['migrations'][version-1]['bundle_size']])
# When integrated, pin the measured projections to the actual selected-card
# adapter so a future loader change cannot silently stale this query fixture.
loader = Path(sys.argv[1]) / 'client/features/labels/mutation.c'
if loader.exists():
    literal = r'"(?:\\.|[^"\\])*"'
    source = ''.join(ast.literal_eval(value) for value in re.findall(literal, loader.read_text()))
    test = (Path(sys.argv[1]) / 'tests/sqlite_schema_v5_test.c').read_text()
    for name in ('labels_query', 'selected_query'):
        query = ast.literal_eval(re.search(r'static const char ' + name + r'\[\] = (' + literal + r');', test).group(1))
        assert query in source, 'Update label query-work fixture to match the actual loader query'

PY
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
 "$root_dir/tests/sqlite_schema_v5_test.c" "$root_dir/server/sqlite_storage.c" \
 "$root_dir/server/sqlite_backup.c" "$root_dir/server/sqlite_restore.c" \
 "$root_dir/server/sha256.c" -lsqlite3 -o "$test_dir/test"
"$test_dir/test" "$test_dir/v1.sql" "$test_dir/v2.sql" "$test_dir/v3.sql" "$test_dir/v4.sql" "$test_dir/v5.sql" "$test_dir"
