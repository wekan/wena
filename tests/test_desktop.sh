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

def domain_content():
    tables = ('actors', 'sessions', 'boards', 'swimlanes', 'lists', 'cards', 'idempotency_keys')
    with sqlite3.connect(path) as db:
        return {table: db.execute('SELECT * FROM ' + table + ' ORDER BY rowid').fetchall()
                for table in tables}

before_domain = domain_content()
env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
exe = str(directory / 'wena-desktop')
# Diagnostics must succeed before database setup and SDL video initialization.
report_cwd = directory / 'read-only-report'
report_cwd.mkdir()
report_cwd.chmod(0o555)
try:
    report_env = dict(env, SDL_VIDEODRIVER='wena-invalid-driver')
    run = subprocess.run([exe, '--dependency-info'], cwd=report_cwd, env=report_env,
                         capture_output=True, text=True, timeout=20)
    assert run.returncode == 0, (run.stdout, run.stderr)
    report = dict(line.split('=', 1) for line in run.stdout.splitlines())
    assert report['format'] == 'wena-dependencies-v1'
    assert report['scope'] == 'libraries_loaded_by_this_process'
    assert report['sqlite_runtime'] and report['sqlite_source_id']
    assert not list(report_cwd.iterdir()), 'diagnostics wrote workspace files'
    help_run = subprocess.run([exe, '--help'], cwd=report_cwd, env=report_env,
                              capture_output=True, text=True, timeout=20)
    assert help_run.returncode == 0 and not help_run.stderr
    for option in ('--database', '--create', '--language', '--dependency-info', '--smoke'):
        assert option in help_run.stdout
    assert not list(report_cwd.iterdir()), 'help wrote workspace files'
    for extra in ('--smoke', '--create', '--help'):
        help_run = subprocess.run([exe, '--help', extra], cwd=report_cwd, env=report_env,
                                  capture_output=True, timeout=20)
        assert help_run.returncode != 0, extra
    for extra in ('--smoke', '--create', '--dependency-info'):
        run = subprocess.run([exe, '--dependency-info', extra], cwd=report_cwd,
                             env=report_env, capture_output=True, timeout=20)
        assert run.returncode != 0, extra
    assert not list(report_cwd.iterdir())
finally:
    report_cwd.chmod(0o755)
valid = ['--database', str(path), '--actor', 'actor', '--board', 'board', '--smoke']
run = subprocess.run([exe, *valid], env=env, capture_output=True, text=True, timeout=20)
assert run.returncode == 0, (run.stdout, run.stderr)
assert 'Wena desktop smoke passed' in run.stdout
assert domain_content() == before_domain
with sqlite3.connect(path) as db:
    assert db.execute('PRAGMA user_version').fetchone() == (6,)
    assert db.execute('SELECT count(*) FROM schema_migrations').fetchone() == (6,)
    assert db.execute('SELECT count(*) FROM card_descriptions').fetchone() == (0,)
before = content()
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
    # Reopen the actual program and transfer focus from List title to Move list.
    # The first list moves after the newly created one; its cards stay scoped.
    move_env = dict(event_env, WENA_TEST_HIERARCHY_MOVE='1')
    run = subprocess.run([exe, '--database', str(event_path), '--actor', 'actor',
                          '--board', 'board', '--language', 'en'], env=move_env,
                         capture_output=True, text=True, timeout=20)
    assert run.returncode == 0, (run.stdout, run.stderr)
    with sqlite3.connect(event_path) as db:
        actual = db.execute('SELECT title,position FROM lists ORDER BY position').fetchall()
        assert actual == [('Sidebar Ää regression', 0), ('List', 1)], actual
        assert db.execute("SELECT list_id FROM cards WHERE id='card'").fetchone() == ('list',)
        assert db.execute('PRAGMA integrity_check').fetchone() == ('ok',)
    for mode in ('save', 'cancel'):
        description_env = dict(event_env, WENA_TEST_DESCRIPTION=mode)
        run = subprocess.run([exe, '--database', str(event_path), '--actor', 'actor',
                              '--board', 'board', '--language', 'en'], env=description_env,
                             capture_output=True, text=True, timeout=20)
        assert run.returncode == 0, (run.stdout, run.stderr)
        with sqlite3.connect(event_path) as db:
            actual = db.execute('SELECT board_id,card_id,description FROM card_descriptions').fetchall()
            assert actual == [('board', 'card', 'First Ää\nSecond Ω')], (mode, actual)
            version = db.execute("SELECT version FROM cards WHERE id='card'").fetchone()
            if mode == 'save':
                saved_version = version
            else:
                assert version == saved_version
    checklist_env = dict(event_env, WENA_TEST_CHECKLIST='1')
    run = subprocess.run([exe, '--database', str(event_path), '--actor', 'actor',
                          '--board', 'board', '--language', 'en'], env=checklist_env,
                         capture_output=True, text=True, timeout=20)
    assert run.returncode == 0, (run.stdout, run.stderr)
    with sqlite3.connect(event_path) as db:
        actual = db.execute('SELECT board_id,card_id,title FROM checklists').fetchall()
        assert actual == [('board', 'card', 'SDL checklist')], actual
        actual = db.execute('SELECT board_id,card_id,title,is_finished FROM checklist_items').fetchall()
        assert actual == [('board', 'card', 'SDL item Ä', 1)], actual
        assert db.execute('PRAGMA foreign_key_check').fetchall() == []
    # Drive the unchanged executable through label create, assign, cancel and
    # inspect on separate process launches. Later launches preserve all rows.
    labels_before = None
    for mode in ('create', 'cancel', 'read'):
        labels_env = dict(event_env, WENA_TEST_LABELS=mode)
        if mode != 'create':
            labels_env['WENA_TEST_CARD_BADGES'] = '1'
        run = subprocess.run([exe, '--database', str(event_path), '--actor', 'actor',
                              '--board', 'board', '--language', 'en'], env=labels_env,
                             capture_output=True, text=True, timeout=20)
        assert run.returncode == 0, (mode, run.stdout, run.stderr)
        with sqlite3.connect(event_path) as db:
            actual = db.execute('SELECT board_id,name,color FROM labels').fetchall()
            assert actual == [('board', 'SDL label Ä', 'white')], (mode, actual)
            actual = db.execute('SELECT board_id,card_id FROM card_labels').fetchall()
            assert actual == [('board', 'card')], (mode, actual)
            assert db.execute('PRAGMA foreign_key_check').fetchall() == []
            current = '\n'.join(db.iterdump())
            if mode == 'create':
                labels_before = current
            else:
                assert current == labels_before, mode + ' unexpectedly wrote label data'
    # Clicking an assigned badge routes directly to its scoped label panel.
    # Editing the same row proves the panel opened and the cache was refreshed.
    run = subprocess.run([exe, '--database', str(event_path), '--actor', 'actor',
                          '--board', 'board', '--language', 'en'],
                         env=dict(event_env, WENA_TEST_LABEL_BADGE='1'),
                         capture_output=True, text=True, timeout=20)
    assert run.returncode == 0, (run.stdout, run.stderr)
    with sqlite3.connect(event_path) as db:
        actual = db.execute('SELECT name FROM labels').fetchall()
        assert actual == [('SDL label Ä badge',)], actual
        assert db.execute('SELECT board_id,card_id FROM card_labels').fetchall() == [('board', 'card')]
    # Count display requires explicit persisted opt-in. Keep the independent
    # collapse fixture unchanged while checking 0/0 -> 0/1 -> 1/1 interaction.
    summary_path = directory / 'summary-board.sqlite'
    # Keep this real-SDL count fixture independent from the label badge fixture:
    # it starts with one plain card and no checklist rows, so the added count row
    # has stable coordinates of its own.
    with sqlite3.connect(path) as source, sqlite3.connect(summary_path) as target:
        source.backup(target)
        summary_before = '\n'.join(target.iterdump())
    summary_args = [exe, '--database', str(summary_path), '--actor', 'actor',
                    '--board', 'board', '--language', 'en']
    summary_env = dict(event_env, WENA_TEST_CHECKLIST_SUMMARY='disabled')
    run = subprocess.run(summary_args, env=summary_env, capture_output=True,
                         text=True, timeout=20)
    assert run.returncode == 0, run.stderr
    with sqlite3.connect(summary_path) as db:
        assert '\n'.join(db.iterdump()) == summary_before, 'disabled counter accepted item mutation'
    settings_after = None
    for mode in ('enable', 'cancel'):
        run = subprocess.run(summary_args,
                             env=dict(event_env, WENA_TEST_BOARD_SETTINGS=mode),
                             capture_output=True, text=True, timeout=20)
        assert run.returncode == 0, (mode, run.stderr)
        with sqlite3.connect(summary_path) as db:
            actual_settings = db.execute('SELECT show_checklist_count FROM board_settings').fetchall()
            assert actual_settings == [(1,)], (mode, actual_settings, run.stdout, run.stderr)
            current = '\n'.join(db.iterdump())
            if mode == 'enable':
                settings_after = current
            else:
                assert current == settings_after, 'cancel changed persisted board setting'
    # Give the opt-in display one empty list. This is deliberately a 0/0
    # fixture: compact counts show existing empty checklists too, and the
    # actual SDL click below must open/close without changing it.
    with sqlite3.connect(summary_path) as db:
        db.execute("INSERT INTO checklists(id,board_id,card_id,title,position,version,created_at,updated_at) VALUES('summary-list','board','card','Summary list',0,1,0,0)")
        summary_open_before = '\n'.join(db.iterdump())
    run = subprocess.run(summary_args,
                         env=dict(event_env, WENA_TEST_CHECKLIST_SUMMARY='open'),
                         capture_output=True, text=True, timeout=20)
    assert run.returncode == 0, run.stderr
    with sqlite3.connect(summary_path) as db:
        assert '\n'.join(db.iterdump()) == summary_open_before
        assert db.execute('PRAGMA integrity_check').fetchone() == ('ok',)
        assert db.execute('PRAGMA foreign_key_check').fetchall() == []
    # Collapse is actor-specific local state and survives an actual process restart.
    assert not list(directory.glob('wena-collapse-*.prefs'))
    collapse_env = dict(event_env, WENA_TEST_COLLAPSE='1')
    args = [exe, '--database', str(event_path), '--actor', 'actor', '--board', 'board', '--language', 'en']
    run = subprocess.run(args, env=collapse_env, capture_output=True, text=True, timeout=20)
    assert run.returncode == 0, run.stderr
    preferences = list(directory.glob('wena-collapse-*.prefs'))
    assert len(preferences) == 1, preferences
    preference_bytes = preferences[0].read_bytes()
    with sqlite3.connect(event_path) as db:
        collapsed_domain = '\n'.join(db.iterdump())
    # This would edit the card if the collapsed list were accidentally visible.
    run = subprocess.run(args, env=dict(event_env, WENA_TEST_DESCRIPTION='save', WENA_TEST_CARD_BADGES='1'),
                         capture_output=True, text=True, timeout=20)
    assert run.returncode == 0, run.stderr
    with sqlite3.connect(event_path) as db:
        assert '\n'.join(db.iterdump()) == collapsed_domain
    run = subprocess.run(args + ['--smoke'], env=env, capture_output=True, text=True, timeout=20)
    assert run.returncode == 0, run.stderr
    assert preferences[0].read_bytes() == preference_bytes
    # A second actor must see the expanded default and can reach the same card.
    with sqlite3.connect(event_path) as db:
        db.execute("INSERT INTO actors VALUES('other-actor','Other',1)")
        version_before = db.execute("SELECT version FROM cards WHERE id='card'").fetchone()[0]
    other_args = [exe, '--database', str(event_path), '--actor', 'other-actor', '--board', 'board', '--language', 'en']
    run = subprocess.run(other_args, env=dict(event_env, WENA_TEST_DESCRIPTION='save', WENA_TEST_CARD_BADGES='1'),
                         capture_output=True, text=True, timeout=20)
    assert run.returncode == 0, run.stderr
    with sqlite3.connect(event_path) as db:
        assert db.execute("SELECT version FROM cards WHERE id='card'").fetchone()[0] == version_before + 1
    assert preferences[0].read_bytes() == preference_bytes
    assert len(list(directory.glob('wena-collapse-*.prefs'))) == 1
    # Read-only startup ignores malformed preferences without repairing them.
    preferences[0].write_bytes(b'not a Wena collapse preference')
    staging = Path(str(preferences[0]) + '.tmp')
    staging.write_bytes(b'foreign incomplete staging')
    run = subprocess.run(args + ['--smoke'], env=env, capture_output=True, text=True, timeout=20)
    assert run.returncode == 0, run.stderr
    assert preferences[0].read_bytes() == b'not a Wena collapse preference'
    assert staging.read_bytes() == b'foreign incomplete staging'
    preferences[0].write_bytes(preference_bytes)
    run = subprocess.run(args + ['--smoke'], env=env, capture_output=True, text=True, timeout=20)
    assert run.returncode == 0, run.stderr
    assert preferences[0].read_bytes() == preference_bytes
    assert staging.read_bytes() == b'foreign incomplete staging'
    print('Linux real SDL sidebar/create, hierarchy move, descriptions, checklists, labels and scoped collapse persistence passed')
else:
    print('Linux LD_PRELOAD event regression not applicable to this platform')
print('Desktop smoke, long paths, explicit initialization, canonical locale seeds and negative startup checks passed')
PY
