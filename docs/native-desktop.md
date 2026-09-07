# Local desktop

The POSIX desktop uses SDL2, Nuklear and Wena's relational schema-v1 SQLite
database. Build with a C compiler, SDL2 development files (`sdl2-config`), SQLite
development files, Python 3 and the pinned Nuklear submodule:

```sh
git submodule update --init
./build.sh build desktop
./dist/desktop/wena-desktop --database /absolute/data/wena.sqlite \
  --actor local-user --board my-board --create --title "My board" --language en
```

The database's parent directory must already exist. `--create` explicitly seeds
one actor, board, list and swimlane. The list and swimlane use canonical labels
in the selected language. Initialization validates inputs, applies the pinned
migration, commits and checks SQLite inside a private adjacent staging directory,
then publishes the closed file without replacing an existing path. Files,
directories and symlinks at the destination are preserved. Concurrent creators
produce exactly one successful publication. The containing directory is assumed
to be trusted; persistence of its directory entry across sudden power loss is
not guaranteed by this initializer.

Reopen the same workspace by omitting `--create` and `--title`:

```sh
./dist/desktop/wena-desktop --database /absolute/data/wena.sqlite \
  --actor local-user --board my-board
```

The output embeds and verifies the pinned migration and full offline translation
catalog. SDL2 and SQLite remain shared host dependencies. Cataloged cross-release
targets still build the earlier bootstrap executable; they do not yet package
this desktop. Direct Meteor/FerretDB database migration is not implemented.

## Local interactions

- Add card in an exact list/swimlane; open details and edit the title; move to a
  selected list/swimlane; archive; restore through Board menu → Archives.
- Add list or swimlane through the toolbar. Rename the board through the toolbar,
  a list through List menu, or a swimlane through its Rename button.
- Collapse or expand lists and swimlanes. Collapse state is session-local.
- Select a language in the toolbar. Labels change immediately; the selection
  persists in `DATABASE_PATH.language`. `--language LOCALE` overrides a saved
  selection; otherwise the OS locale is used on first launch. The preference is
  disabled for database paths too long for its bounded settings writer. A failed
  settings write keeps the previous language and displays an error.

Titles accept 1–128 UTF-8 bytes. Empty, overlong, malformed UTF-8 and control
character input cannot be saved. Editors retain unsuccessful input and offer
Cancel/Close. Mutations validate actor and exact board/parent scope, optimistic
versions where applicable, and durable request identities. Idempotency replay is
rejected. Model arrays change only after a successful SQLite commit. Moving a
card appends it in the target column immediately and preserves that order on
reopen. Lists are board-wide and render in every swimlane.

The snapshot has explicit limits of 64 swimlanes, 128 lists and 2048 cards
(including archived cards). Loading fails rather than truncating invalid or
over-capacity data. New objects are rejected before database mutation if their
cache has no space. Changes by another process are not continuously synchronized;
reopen a panel to obtain an authoritative version and restart to reload the full
hierarchy after external changes.

## Trust and remaining scope

The OS user selects the existing local actor and board. Actor existence is
checked, but this is not login or per-board membership authorization. Existing
databases undergo a read-only scope preflight before the verified storage startup
gates. Missing files, malformed records, unknown scope and invalid artifact
payloads fail closed. Identifiers use bounded ASCII letters, digits, underscore
and hyphen. Do not expose this command as a remote launch service.

Activities, members and labels still have scaffold sidebar content. Descriptions,
comments, checklists, attachments, due dates, membership/authentication, native
list/swimlane movement and drag/drop, import/export and remote REST are unfinished.
Canonical runtime strings cover the implemented shared UI contract in every
catalog language; full translation of feature-specific messages, Unicode fonts,
bidirectional shaping, RTL geometry, touch and accessibility parity remain open.

## Verification

```sh
./build.sh tests all
./build.sh tests sanitizers
SDL_VIDEODRIVER=dummy ./dist/desktop/wena-desktop \
  --database /absolute/data/wena.sqlite --actor local-user \
  --board my-board --smoke
```

Smoke renders three real SDL/Nuklear frames with mutation callbacks and preference
writes disabled. Adding `--create` explicitly allows workspace initialization
before those read-only frames. Storage startup can configure WAL; existing-data
smoke assertions compare schema and domain contents, not journal bytes.

The suite covers startup, creation, input bounds, malformed arguments, wrong
scope, conflicts, replay, injected rollback, concurrent creation and persistence
after reopening. Real Nuklear tests consume draw commands after every frame,
including mouse-down frames, matching the SDL renderer's lifecycle. Optional
ASan/UBSan tests default to leak detection; environments that cannot support
LeakSanitizer may set `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` and must report
that limitation. See [latest session report](work-session-02.md).
