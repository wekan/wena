#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir="${TMPDIR:-/tmp}/wena-hierarchy-move-sqlite-$$"
mkdir -p "$test_dir"
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
  -I"$root_dir/tests/fakes" \
  "$root_dir/tests/hierarchy_move_sqlite_test.c" \
  "$root_dir/tests/fakes/nuklear.c" \
  "$root_dir/client/features/card_details.c" \
  "$root_dir/client/features/hierarchy_move.c" \
  "$root_dir/client/components/cards/card_details_canvas.c" \
  "$root_dir/client/features/hierarchy_move_mutation.c" \
  "$root_dir/imports/ui/page_contract.c" \
  "$root_dir/models/card.c" "$root_dir/models/model.c" \
  "$root_dir/models/board.c" "$root_dir/models/list.c" "$root_dir/models/swimlane.c" \
  "$root_dir/server/sqlite_persistence.c" \
  "$root_dir/server/sqlite_board.c" \
  "$root_dir/server/sqlite_storage.c" "$root_dir/server/sha256.c" \
  "$root_dir/server/region_response.c" -lsqlite3 -o "$test_dir/test"
python3 - "$root_dir/server/migrations/001_initial.sql" "$test_dir/fixture.sqlite" <<'PYCODE'
import sqlite3, sys
with sqlite3.connect(sys.argv[2]) as db:
    db.executescript(open(sys.argv[1]).read())
    db.executescript("""
        INSERT INTO actors VALUES('actor','Actor',1);
        INSERT INTO boards VALUES('board','Board',1);
        INSERT INTO lists VALUES('first-list','board','First',0,1);
        INSERT INTO lists VALUES('second-list','board','Second',1,1);
        INSERT INTO swimlanes VALUES('first-lane','board','First',0,1);
        INSERT INTO swimlanes VALUES('second-lane','board','Second',1,1);
    """)
PYCODE
"$test_dir/test" "$test_dir/fixture.sqlite"
