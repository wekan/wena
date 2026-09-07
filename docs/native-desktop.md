# Optional local desktop

The optional desktop executable opens an **existing Wena relational schema-v1
SQLite database** and displays its board through SDL2 and Nuklear. It is separate
from the release-target bootstrap executables. It does not initialize a workspace,
import a Meteor/FerretDB database, or provide remote authentication.

Build on a POSIX host with a C compiler, SDL2 development files (`sdl2-config`),
SQLite development files, Python 3, and the pinned Nuklear submodule:

```sh
sh scripts/build_desktop.sh /absolute/output/wena-desktop
/absolute/output/wena-desktop --database /absolute/data/wena.sqlite \
  --actor existing_actor_id --board existing_board_id
```

The output embeds and verifies the same pinned migration and offline translation
catalog as other Wena artifacts. SDL2 and SQLite remain shared host dependencies;
this host build is not a new verified cross-release target. Parent directories for
the output must already exist.

The operating-system user is trusted to select the existing local actor and board.
Actor existence is checked, but this CLI is **not an authentication mechanism** or
a per-board membership authorization system. Do not expose it as a remote launch
service. The process first checks actor and board data through a read-only database
connection, then uses the verified migration/storage startup gates before opening
the interactive view. Missing files, unknown scope, malformed records and invalid
artifact payloads fail closed. Identifiers use the current adapter's bounded ASCII
letters, digits, underscore and hyphen convention.

Implemented interactions include opening card details, bounded title editing with
Save/Cancel, archiving a card, board-menu/sidebar state, and list/swimlane collapse.
Title and archive writes use the shared optimistic, idempotent SQLite transaction
adapter and refresh the in-memory card only after commit. Conflicts retain the edit
or report a generic status; cancel/reopen details to load the current title/version.
The board hierarchy is loaded at startup, so changes made by another process are
not continuously synchronized. Existing Add card, list menu and sidebar content
actions remain unimplemented intents. Keyboard text entry uses SDL text-input
handling; complete accessibility, touch, drag/drop, theme, RTL, translated runtime
labels and full Unicode font coverage remain future work.

Headless startup verification uses three frames and disables mutation callbacks:

```sh
SDL_VIDEODRIVER=dummy /absolute/output/wena-desktop \
  --database /absolute/data/wena.sqlite --actor existing_actor_id \
  --board existing_board_id --smoke
sh tests/test_desktop.sh
```

The suite builds the real executable, creates an isolated valid fixture, verifies
successful SDL/Nuklear rendering, rejects invalid arguments and absent/unknown
scope, and compares the logical database contents before and after smoke. SQLite
startup may configure WAL and perform storage checks; the smoke assertion concerns
schema and domain data, not byte-for-byte journal identity. Separate editor and
adapter suites cover title/archive input, version conflicts, transaction rollback,
replay and persistence after reopening SQLite.
