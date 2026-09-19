#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/wena-checklist-summary-XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
python3 - "$root_dir" "$test_dir" <<'PY'
from pathlib import Path
import sys
sys.path.insert(0, str(Path(sys.argv[1]) / 'scripts'))
from verify_migrations import verify
lock, bundle = verify(Path(sys.argv[1]))
(Path(sys.argv[2]) / 'schema.sql').write_bytes(bundle)
PY
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
 "$root_dir/tests/checklist_summary_test.c" "$root_dir/client/features/checklists/summary.c" \
 "$root_dir/models/model.c" "$root_dir/models/checklist.c" "$root_dir/models/checklist_item.c" \
 "$root_dir/server/sqlite_storage.c" "$root_dir/server/sha256.c" -lsqlite3 -o "$test_dir/test"
"$test_dir/test" "$test_dir/schema.sql" "$test_dir/summary.sqlite"
