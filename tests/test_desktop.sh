#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir="${TMPDIR:-/tmp}/wena-desktop-test-$$"
mkdir -p "$test_dir"
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
sh "$root_dir/scripts/build_desktop.sh" "$test_dir/wena-desktop"
python3 - "$root_dir" "$test_dir" <<'PY'
import hashlib
import json
import shutil
import shlex
import zlib
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
# Explicit creation is the only startup path allowed to initialize a workspace.
new = directory/'new-workspace.sqlite'
args = ['--database', str(new), '--actor', 'local-user', '--board', 'new-board',
        '--create', '--title', 'Uusi taulu', '--language', 'fi', '--smoke']
run = subprocess.run([exe, *args], env=env, capture_output=True, text=True, timeout=20)
assert run.returncode == 0, run.stderr
raw = (root/'imports/i18n/wekan-i18n.bin').read_bytes()
canonical = json.loads(zlib.decompress(raw[20:]))
fi = dict(next(item for item in canonical['languages'] if item['tag'] == 'fi')['entries'])
with sqlite3.connect(new) as db:
    assert db.execute('SELECT title FROM boards').fetchone() == ('Uusi taulu',)
    assert db.execute('SELECT id,title FROM lists').fetchone() == ('default-list', fi['list'])
    assert db.execute('SELECT id,title FROM swimlanes').fetchone() == ('default-lane', fi['swimlane'])
    assert db.execute('PRAGMA integrity_check').fetchone() == ('ok',)
    seeded = '\n'.join(db.iterdump())
assert not Path(str(new)+'.language').exists(), 'smoke must not write language preference'
run = subprocess.run([exe, *args], env=env, capture_output=True, timeout=20)
assert run.returncode != 0, 'explicit creation must not overwrite an existing workspace'
with sqlite3.connect(new) as db:
    assert '\n'.join(db.iterdump()) == seeded
bad_new = directory/'bad-new.sqlite'
run = subprocess.run([exe, '--database', str(bad_new), '--actor', 'actor',
                      '--board', 'board', '--create', '--title', '', '--smoke'],
                     env=env, capture_output=True, timeout=20)
assert run.returncode != 0 and not bad_new.exists()
assert not list(directory.glob('.wena-workspace-*'))
# Language preference sidecar bounds must not prevent opening a valid database.
for length in (497, 498, 501, 502):
    nested = directory / ('long-' + str(length))
    nested.mkdir()
    while length - len(str(nested)) - 1 > 200:
        nested = nested / ('d' * 100)
        nested.mkdir()
    long_path = nested / ('b' * (length - len(str(nested)) - 1))
    assert len(str(long_path)) == length
    shutil.copyfile(path, long_path)
    run = subprocess.run([exe, '--database', str(long_path), '--actor', 'actor',
                          '--board', 'board', '--smoke'], env=env,
                         capture_output=True, text=True, timeout=20)
    assert run.returncode == 0, (length, run.stderr)
    assert not Path(str(long_path) + '.language').exists()
    assert not Path(str(long_path) + '.language.tmp').exists()

# LD_PRELOAD is Linux-specific; all other desktop checks remain portable.
if sys.platform.startswith('linux'):
    interposer = directory / 'desktop-events.so'
    flags = shlex.split(subprocess.check_output(['sdl2-config', '--cflags', '--libs'], text=True))
    subprocess.run(['cc', '-std=c89', '-pedantic-errors', '-Wall', '-Wextra', '-Werror',
                    '-fPIC', '-shared', str(root/'tests/desktop_events_test.c'),
                    '-o', str(interposer), *flags], check=True)
    event_path = directory / 'event-board.sqlite'
    shutil.copyfile(path, event_path)
    event_env = dict(env, LD_PRELOAD=str(interposer))
    run = subprocess.run([exe, '--database', str(event_path), '--actor', 'actor',
                          '--board', 'board', '--language', 'en'], env=event_env,
                         capture_output=True, text=True, timeout=20)
    assert run.returncode == 0, (run.stdout, run.stderr)
    with sqlite3.connect(event_path) as db:
        actual = db.execute('SELECT title FROM lists ORDER BY title').fetchall()
        assert actual == [('List',), ('Sidebar Ää regression',)], actual
        assert db.execute('SELECT COUNT(*) FROM cards').fetchone() == (1,)
        assert db.execute('PRAGMA integrity_check').fetchone() == ('ok',)
    print('Linux real SDL sidebar-to-Add-list event regression passed')
else:
    print('Linux LD_PRELOAD event regression not applicable to this platform')
print('Desktop smoke, long paths, explicit initialization, canonical locale seeds and negative startup checks passed')
PY
