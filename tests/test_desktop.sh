#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir="${TMPDIR:-/tmp}/wena-desktop-test-$$"
mkdir -p "$test_dir"
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
sh "$root_dir/scripts/build_desktop.sh" "$test_dir/wena-desktop"
python3 - "$root_dir" "$test_dir" <<'PY'
import hashlib
from pathlib import Path
import os
import sqlite3
import subprocess
import sys

root, directory = map(Path, sys.argv[1:])
path = directory / 'board.sqlite'
sql = (root / 'server/migrations/001_initial.sql').read_bytes()
with sqlite3.connect(path) as db:
    db.executescript(sql.decode())
    db.execute('INSERT INTO schema_migrations VALUES(1,?,1)', (hashlib.sha256(sql).hexdigest(),))
    db.execute('PRAGMA user_version=1')
    db.executescript("""
        INSERT INTO actors VALUES('actor','Local user',1);
        INSERT INTO boards VALUES('board','Desktop board',1);
        INSERT INTO swimlanes VALUES('lane','board','Lane',0,1);
        INSERT INTO lists VALUES('list','board','List',0,1);
        INSERT INTO cards VALUES('card','board','lane','list','Persisted card',0,0,1);
    """)

def content():
    with sqlite3.connect(path) as db:
        return '\n'.join(db.iterdump())

before = content()
env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
exe = str(directory / 'wena-desktop')
valid = ['--database', str(path), '--actor', 'actor', '--board', 'board', '--smoke']
run = subprocess.run([exe, *valid], env=env, capture_output=True, text=True, timeout=20)
assert run.returncode == 0, (run.stdout, run.stderr)
assert 'Wena desktop smoke passed' in run.stdout
assert content() == before
for args in [[], ['--unknown'], valid + ['--smoke'], valid + ['--actor', 'actor'],
             ['--database', str(path)],
             ['--database', 'relative.sqlite', '--actor', 'actor', '--board', 'board', '--smoke'],
             ['--database', str(directory/'missing.sqlite'), '--actor', 'actor', '--board', 'board', '--smoke'],
             ['--database', str(directory), '--actor', 'actor', '--board', 'board', '--smoke'],
             ['--database', str(path), '--actor', 'unknown', '--board', 'board', '--smoke'],
             ['--database', str(path), '--actor', 'actor', '--board', 'unknown', '--smoke'],
             ['--database', str(path), '--actor', 'bad/actor', '--board', 'board', '--smoke']]:
    run = subprocess.run([exe, *args], env=env, capture_output=True, timeout=20)
    assert run.returncode != 0, args
    assert content() == before
assert not (directory/'missing.sqlite').exists()
# A corrupt existing file must stay unchanged; startup may not initialize it.
corrupt = directory/'corrupt.sqlite'
corrupt.write_bytes(b'not a sqlite database')
run = subprocess.run([exe, '--database', str(corrupt), '--actor', 'actor',
                      '--board', 'board', '--smoke'], env=env, capture_output=True, timeout=20)
assert run.returncode != 0
assert corrupt.read_bytes() == b'not a sqlite database'
print('Desktop executable smoke, argument/scope failures, and unchanged domain data checks passed')
PY
