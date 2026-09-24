#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/wena-card-people-store-XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
python3 - "$root_dir" "$test_dir/schema.sql" <<'PY'
import sys
from pathlib import Path
sys.path.insert(0,str(Path(sys.argv[1])/'scripts'))
from verify_migrations import verify
Path(sys.argv[2]).write_bytes(verify(Path(sys.argv[1]))[1])
PY
test_source=${WENA_PEOPLE_TEST_SOURCE:-card_people_store_test.c}
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror ${WENA_TEST_CFLAGS:-} \
 "$root_dir/tests/$test_source" "$root_dir/server/card_people_store.c" \
 "$root_dir/server/mutations/card_people.c" "$root_dir/server/list_state.c" \
 "$root_dir/models/card_people.c" "$root_dir/models/model.c" \
 "$root_dir/server/sqlite_storage.c" "$root_dir/server/sha256.c" \
 -lsqlite3 -o "$test_dir/test"
"$test_dir/test" "$test_dir/schema.sql" "$test_dir/people.sqlite"
