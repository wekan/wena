# Local desktop

The POSIX desktop uses SDL2, Nuklear and Wena's relational schema-v7 SQLite
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

The output embeds and verifies the pinned migration chain and full offline translation
catalog. SDL2 and SQLite remain shared host dependencies. Cataloged cross-release
targets still build the earlier bootstrap executable; they do not yet package
this desktop. Direct Meteor/FerretDB database migration is not implemented.
Existing Wena schema-v1 databases upgrade atomically through the immutable v1-v7
chain on storage startup, including smoke mode. A verified old backup is upgraded
in a private staged copy before restore interrupts the running database.

## Local interactions

- Add card in an exact list/swimlane; open details and edit the title; move to a
  selected list/swimlane and ordinal position; archive; restore through Board
  menu → Archives. Description opens a multiline editor with explicit Save.
- Add list or swimlane through the toolbar. Rename the board through the toolbar,
  a list through List menu, or a swimlane through its Rename button. Those list
  and swimlane panels also offer explicit movement to another sibling position.
- Collapse or expand lists and swimlanes. The desktop remembers collapsed IDs
  in a separate preference file scoped to the exact workspace path, actor and
  board. Invalid or unwritable preferences leave the current session usable;
  a contextual Settings / Collapse error offers Save to retry. Startup and smoke
  never rewrite preferences. See [collapse preferences](collapse-preferences.md).
- Apply a session-local card-title filter. Matching is a literal substring,
  case-insensitive for ASCII and byte-exact for other UTF-8 characters. This is
  an explicit subset of WeKan's regular-expression search. Clear restores all
  active cards; filtering retains the complete snapshot and exact parent scope.
- Open Checklists from card details. Create/rename checklists, add/rename items,
  toggle completion with explicit Save, and hide checked or all items. Progress
  counts all items, including hidden ones; display choices do not change completion.
  A list/item Rename/Edit form also offers Delete, followed by a separate explicit
  confirmation. A whole-list confirmation includes its children. Deletion is
  permanent; Cancel/Escape cancel and Enter cannot confirm.
- From checklist Rename, choose Move Checklist and an active destination card
  on the same board. Titles include IDs to distinguish identically named cards.
  Save appends the checklist with all children, including hidden/completed items.
  Both cards' captured versions must still match; Cancel/Escape write nothing.
  Destination selection loads once; idle drawing does not issue SQL. A destination
  already at 64 checklists, above the combined 1024-item limit, or with its last
  checklist at the maximum position is rejected without changes.
- From item Edit, choose Destination, then a card and checklist on the same
  board. The current card is allowed; the current checklist is excluded. Save
  appends the item and preserves its completion state. Both parent checklists,
  the item and affected cards must still match their captured revisions.
  Cancel/Escape discard selection; a failed reload after Save only retries reads.
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
card can append or use a selected target ordinal. Indexed moves include archived
rows and compact existing gaps atomically; unchanged order does not rewrite data.
List/swimlane movement also validates the complete sibling-order fingerprint.
Lists are board-wide and render in every swimlane.

Focused single-line title fields submit with Enter. Escape cancels the focused
editor. Plain Enter inserts a newline in descriptions, which accept empty text
or up to 1024 UTF-8 bytes; LF, CR and tab are permitted, other controls rejected.
Description changes share the card's optimistic version with title/move/archive.
Reopening an editor reloads authoritative content and version. These explicit
bounds are native storage limits, not unbounded WeKan field parity.

Checklist panels load a complete separate snapshot with at most 64 checklists
and 1024 items per card. Titles have the same 128-byte bound. Every mutation
checks the card's aggregate version plus applicable checklist/item versions.
Unchanged saves leave versions and idempotency records untouched. After a commit,
the panel reloads its data; if that read fails, editing is disabled and Refresh
retries only the read, so it cannot duplicate the accepted write. Timestamps are
UTC epoch milliseconds with the current local clock's one-second precision.

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

Activities and members still have scaffold sidebar content. Labels and card
assignments have native editors and cached board badges. Comments,
attachments, due dates, membership/authentication, native drag/drop, import/export
and remote REST are unfinished. Checklist/item same-card reordering, atomic batch entry and whole-checklist
same-board transfer and item transfers between checklists on the same board are
implemented. Cross-board transfer remains open. Compact counts have a default-off
board setting. Expanded previews show checklist titles and visible items using the
canonical true default, configurable in Board Settings; checklist Actions offers
Default, Yes and No overrides, which take priority over the board preference.
Hidden/all-completed display flags are respected, and a title click opens the
card's checklist editor. Preview checkboxes save completion through the same guarded editor mutation.
A rejected save shows an error and requires Refresh before another preview edit;
refresh never retries a mutation. Inline title/add editing, per-user checklist
collapse and drag/drop remain open.
Canonical runtime strings cover the implemented shared UI contract in every
catalog language, including the current feature messages. The trusted embedded
Apache-2.0 Roboto asset covers selected Latin, Greek and Cyrillic glyphs. Full
Unicode fonts, bidirectional shaping, RTL geometry, touch and accessibility parity
remain open. See the [component gap inventory](native-parity-gaps.md).

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
smoke assertions allow the verified schema upgrade, preserve domain contents and
require unchanged schema/content on subsequent reopen, not identical journal bytes.

The suite covers startup, creation, input bounds, malformed arguments, wrong
scope, conflicts, replay, injected rollback, concurrent creation and persistence
after reopening. Real Nuklear tests consume draw commands after every frame,
including mouse-down frames, matching the SDL renderer's lifecycle. Optional
ASan/UBSan tests default to leak detection; environments that cannot support
LeakSanitizer may set `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` and must report
that limitation. See [latest session report](work-session-03.md).
