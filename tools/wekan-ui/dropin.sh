#!/usr/bin/env sh
# Wena as a drop-in for a WeKan FerretDB bundle, checked with WeKan itself:
#
#   1. FerretDB on an empty wekan-files/db; a WeKan user is made through it.
#   2. Wena opens those wekan-files as that user - its first run makes "My
#      board" - and adds a list, a card, a description and a checklist.
#   3. WeKan's bundle runs on the same files through FerretDB, and its own
#      page shows the board on All Boards, and the list and card on it; then
#      WeKan's UI makes a board "Made in WeKan" with a list and a card.
#   4. With WeKan stopped, Wena reads that board in and opens it.
#
#   tools/wekan-ui/dropin.sh
#
# Needs: the WeKan checkout this is in (WEKAN_ROOT), its built bundle
# (.build/bundle with programs/server's npm dependencies), its Playwright,
# FerretDB (FERRETDB_BIN, default ../FerretDB/bin/ferretdb) and a built
# dist/desktop/wena-desktop. Screenshots go to WENA_CAPTURE_DIR.
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
wena=$(CDPATH= cd -- "$here/../.." && pwd)
wekan=${WEKAN_ROOT:-$(CDPATH= cd -- "$wena/../.." && pwd)}
ferretdb=${FERRETDB_BIN:-$wena/../FerretDB/bin/ferretdb}
capture=${WENA_CAPTURE_DIR:-$here/capture}
work=$(mktemp -d "${TMPDIR:-/tmp}/wena-dropin-XXXXXX")
db_port=27961
web_port=3361
pids=
cleanup() { for pid in $pids; do kill "$pid" 2>/dev/null || true; done; rm -rf "$work"; }
trap cleanup EXIT HUP INT TERM
wait_port() {
  i=0
  until nc -z 127.0.0.1 "$1" 2>/dev/null; do i=$((i + 1)); [ "$i" -lt "$2" ] || return 1; sleep 1; done
}
mkdir -p "$work/files/db" "$capture"
"$ferretdb" --handler=sqlite --sqlite-url="file:$work/files/db/" --listen-addr="127.0.0.1:$db_port" \
  --telemetry=disable --log-level=error > "$work/ferretdb.log" 2>&1 &
ferret=$!; pids="$ferret"
wait_port "$db_port" 30
user=$(cd "$wekan/tests/playwright" && WEKAN_MONGO_URL="mongodb://127.0.0.1:$db_port/wekan" \
  node -e "const db = require('./helpers/db'); const u = db.seedUser({ isAdmin: true }); console.log(JSON.stringify(u))")
kill "$ferret"; wait "$ferret" 2>/dev/null || true; pids=
username=$(printf '%s' "$user" | node -e "process.stdin.on('data', d => console.log(JSON.parse(d).username))")
echo "WeKan user $username made through FerretDB"

WRITABLE_PATH="$work" WENA_USER="$username" WENA_LOG_DIR="$work" "$wena/dist/desktop/wena-desktop" --smoke
cc -std=c89 -I"$wena" "$here/dropin_tool.c" "$wena/server/wekan_sync.c" "$wena/server/ferretdb_sqlite.c" \
  "$wena/server/sqlite_storage.c" "$wena/server/sha256.c" "$wena/server/sqlite_board.c" "$wena/models/model.c" \
  "$wena/models/board.c" "$wena/models/swimlane.c" "$wena/models/list.c" "$wena/models/card.c" \
  "$wena/models/wip_limit.c" "$wena/models/color.c" -lsqlite3 -o "$work/tool"
"$work/tool" "$work" "$username"
echo "Wena wrote its board, list, card and checklist"

"$ferretdb" --handler=sqlite --sqlite-url="file:$work/files/db/" --listen-addr="127.0.0.1:$db_port" \
  --telemetry=disable --log-level=error > "$work/ferretdb.log" 2>&1 &
pids="$!"
wait_port "$db_port" 30
(cd "$wekan/.build/bundle" && PORT=$web_port ROOT_URL="http://localhost:$web_port" \
  MONGO_URL="mongodb://127.0.0.1:$db_port/wekan" WRITABLE_PATH="$work" exec node main.js > "$work/wekan.log" 2>&1) &
pids="$pids $!"
wait_port "$web_port" 240 || { tail -40 "$work/wekan.log" >&2; exit 1; }
echo "WeKan serving the same files on port $web_port"
cd "$wekan"
WENA_DROPIN_USER="$user" WENA_CAPTURE_DIR="$capture" WEKAN_ROOT="$wekan" WEKAN_BASE_URL="http://localhost:$web_port" \
  PLAYWRIGHT_BROWSERS_PATH="${PLAYWRIGHT_BROWSERS_PATH:-$wekan/.tools/ms-playwright}" \
  npx --prefix "$wekan/tests/playwright" playwright test --config "$here/playwright.config.js" "$here/dropin.e2e.js"

for pid in $pids; do kill "$pid" 2>/dev/null || true; done
for pid in $pids; do wait "$pid" 2>/dev/null || true; done
pids=
board=$(cat "$capture/wekan-board.txt")
"$work/tool" "$work" "$username" check "$board"
rm -f "$work/desktop.log"
WRITABLE_PATH="$work" WENA_USER="$username" WENA_LOG_DIR="$work" "$wena/dist/desktop/wena-desktop" \
  --show "open:$board" --screenshot "$capture/dropin-wena-opens-wekan-board.bmp"
grep -q "board $board" "$work/desktop.log" || { cat "$work/desktop.log" >&2; exit 1; }
echo "Wena opened the board, list and card WeKan's UI made"
