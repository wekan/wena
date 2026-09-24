#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/wena-schema-v7-XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
schema_version=${WENA_TEST_SETTING_VERSION:-7}
schema_table=${WENA_TEST_SETTING_TABLE:-board_minicard_settings}
schema_column=${WENA_TEST_SETTING_COLUMN:-show_checklists}
python3 - "$root_dir" "$test_dir" "$schema_version" <<'PY'
from pathlib import Path
import sys
sys.path.insert(0, str(Path(sys.argv[1]) / 'scripts'))
from verify_migrations import verify
lock, bundle = verify(Path(sys.argv[1]))
for version in range(1, int(sys.argv[3])+1):
    (Path(sys.argv[2]) / ('v%d.sql' % version)).write_bytes(bundle[:lock['migrations'][version-1]['bundle_size']])
PY
archive_flag=
if [ "$schema_version" = 10 ]; then archive_flag=-DWENA_SETTING_LIST_ARCHIVE; fi
if [ "$schema_version" = 11 ]; then archive_flag=-DWENA_SETTING_COLORS; fi
cc $archive_flag "-DWENA_SETTING_SCHEMA_VERSION=$schema_version" \
 "-DWENA_SETTING_TABLE=\"$schema_table\"" "-DWENA_SETTING_COLUMN=\"$schema_column\"" \
 -std=c89 -pedantic-errors -Wall -Wextra -Werror \
 "$root_dir/tests/sqlite_schema_v7_test.c" "$root_dir/server/sqlite_storage.c" \
 "$root_dir/server/sha256.c" "$root_dir/models/color.c" -lsqlite3 -o "$test_dir/test"
set --
version=1
while [ "$version" -le "$schema_version" ]; do
  set -- "$@" "$test_dir/v$version.sql"
  version=$((version + 1))
done
"$test_dir/test" "$@" "$test_dir"
