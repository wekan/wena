# Development session — 2026-09-07

Starting repository: https://github.com/wekan/wena, upstream commit `51f8ad1`.
The existing `e2e5804` migration-hardening work was retained. This session resumed
its documented title-edit checkpoint and integrated independent feature, SQLite,
JavaScript and test/build workstreams locally. No GitHub write, push, PR, tag,
release or release-workflow execution was performed.

## Implemented and verified

- Bounded native title input with Save/Cancel/Close and authoritative SQLite
  title/version loading; exact 128-byte UTF-8 limit, controls/invalid Unicode
  rejection, stale/scope/replay/rollback/reopen tests and commit-only model refresh.
  Real Nuklear exposed the capacity-includes-NUL boundary, now regression tested.
- Native Archive card callback with snapshot version, atomic persistence,
  conflict/error retention and successful close/cache invalidation.
- Board-scoped, bounded swimlane/list collapse, nested restore and stale pruning.
  Board-wide SQLite lists render in every active lane; widget IDs distinguish
  rendered lanes. Explicit row geometry and scrolling fix clipped card content.
- Owned, bounded SQLite board snapshots with deterministic ordering, strict type,
  identifier, UTF-8 and relationship validation, no partial outputs or truncation.
  A concurrent WAL writer proves snapshot consistency between table reads.
- Optional POSIX SDL2/Nuklear desktop entry point for an existing Wena database,
  existing actor and board. It loads verified migration/catalog payloads and
  composes the model, editor, persistence, sidebar and collapse modules.
- Bounded percent/plus form decoding and strict overflow-checked decimal parsing
  across all nine current SQLite operations; malformed commands consume no keys
  and leave no partial writes or responses.
- Actual emitted enhancement JavaScript executed in a Node VM with a small DOM
  harness: 13 behavioral groups. Fixed late responses mutating after timeout and
  synchronous transport failures retaining in-flight state.
- Central test catalog, bounded parallel independent suites, serialized shared
  artifact builds, explicit prerequisite skips, timeout/failure reporting and
  Python interpreter dispatch; separate `build desktop` host command.

## Validation

`./build.sh tests all`: **49 PASS, 0 FAIL, 0 SKIP**. See the captured
[test output](test-results-2026-09-07.txt). These are suite counts, not individual
assertion counts. `./build.sh build desktop` and the existing Linux amd64 host
bootstrap build passed. The desktop smoke test executes the real binary through
SDL's dummy driver and compares logical database contents before and after.

Seven suites also passed with `-fsanitize=undefined -fno-sanitize-recover=all`:
`card-mutation`, `card-editor-sqlite`, `sqlite-board`, `sqlite-form-validation`,
`nuklear-editor`, `collapse`, and `nuklear-board`. C test builds use
`-std=c89 -pedantic-errors -Wall -Wextra -Werror`.

The environment supplied GCC, Python and Node. Standard development packages were
not available through its package manager. Tests therefore used the existing
system SQLite 3.45.1 and SDL2 2.30.0 shared libraries with matching official
upstream header sources in a temporary external toolchain directory. SQLite's
header template received only version/source-ID substitutions; no application
or dependency behavior was mocked. These temporary toolchain sources are not
part of Wena and no new production dependency was introduced. On a normal POSIX
host, install the SDL2 and SQLite development packages instead. Initialize the
pinned Nuklear submodule, then run the documented commands.

For source parity, `WEKAN_ROOT` referenced an external WeKan checkout at
`689a393841f08c3a020a4ef435b869b7641b21df`. Target-specific cross-compilers and
real-browser E2E were not exercised. JavaScript VM tests do not establish browser
DOM/layout parity; Nuklear command tests do not establish full theme/accessibility
or touch behavior.

## Remaining work and next checkpoint

Wena is **not a complete WeKan replacement**. The next native slice is Add card:
carry exact selected board/list/rendered-lane scope into a bounded editor and
transactional create callback, without the current server callback's implicit
first-list/lane choice. Then complete list menus, sidebar mutations and movement.

First-run workspace/board creation, full GUI cross-release packaging, runtime
translation lookup/fonts/RTL, responsive/accessibility/touch parity, remote REST
and provider authentication compatibility, import/export and direct FerretDB
layout migration remain open. Local actor selection is OS-user trust, not login
or membership authorization. The optional desktop currently loads board data at
startup; it is not a live synchronization client. Existing target-catalog builds
remain bootstrap executables. See [native desktop](native-desktop.md) and
[ROADMAP](../ROADMAP.md) for the precise supported boundary.
