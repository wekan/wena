# Description and checklist schema design

Status: schema-v2 description storage and schema-v3 checklist storage are
implemented. Native mutations/UI are integrated separately. Checklist schema
decisions and evidence are recorded in [schema-v3-storage.md](schema-v3-storage.md);
remaining behavior below is a proposal unless covered by that implemented slice. Reviewed against
local Wena `0a61868` and WeKan `689a393841f08c3a020a4ef435b869b7641b21df`.
The existing v1 migration remains
byte-for-byte unchanged, including its checksum
`e4760a2b70d6651ee84dce93642ccdd4ce8991b488dece5d231e66053f065da5`.

The schema-1 prerequisite is now implemented: `sqlite_storage.c` owns the
compiled ordered registry, and open/backup/restore share transaction-aware
typed history validation. Regression tests cover malformed metadata and
atomic failure. The format-2 lock and generated C89 registry now recognize both
the exact historical v1 payload and the v1+v2 bundle, with the same footer
envelope. Restore upgrades a verified private copy before stopping the listener.
Exact compiled SQL for the new v2 table and index also guards missing or altered
physical objects; a full historical v1 schema fingerprint remains future work.

## Recommended delivery order

1. Completed: ordered compiled migration registry, verified artifact bundle,
   additive description storage in migration 002, and backup/restore gates.
2. Connect an on-demand native description editor to the existing guarded
   card mutation adapter. Keep the board snapshot small.
3. Add checklist and checklist-item models, storage, mutations and native
   card-details controls as the next complete slice. Prefer migration 003
   at that point over shipping unused checklist tables in migration 002.

This order keeps every installed schema tied to a delivered feature and avoids
a second persistence framework. Checklist work can be prepared independently
against the interfaces below while description integration is tested.

## Existing v1 gates that must move together

| Location | Current assumption | Required change |
| --- | --- | --- |
| `server/sqlite_storage.c` | One compiled SQL literal/hash; only versions 0 and 1 | Ordered compiled records, shared metadata validator, atomic forward migration |
| `config/migrations-lock.json` | One migration path/hash/size and schema version 1 | Append-only ordered entries and a deterministic bundle checksum |
| `scripts/verify_migrations.py` | Exact format-1 object and one SQL file | Verify contiguous versions, paths, bytes, sizes, hashes and compiled registry |
| `scripts/embed_migrations.py` | Appends only migration 001 | Append the verified current bundle using the existing footer envelope |
| `server/embedded_migration.c` | Verifies envelope SHA/size but does not interpret SQL | Keep transport verification; compiled registry validates executable payload before any database open |
| `server/sqlite_backup.c` | Copied database must have `user_version == 1` | Shared full metadata validation for supported schemas |
| `server/sqlite_restore.c` | Backup must be v1 with the supplied v1 checksum | Resolve a compiled target, validate the backup history, upgrade a private copy before stopping the listener |
| Runtime, workspace, desktop callers | Pass one payload/hash into `wena_sqlite_open` | Continue using that API; current artifacts carry the current verified bundle |
| Schema/embedding/storage/backup/restore tests | v1-only assertions and fixtures | Retain historical v1 fixtures and add v1-to-v2 and v2 artifact coverage |

`docs/sqlite-storage.md` currently ends with an outdated claim that the adapter
is pending. Replace that paragraph when implementing this slice and distinguish
verified behavior from the durability goals that are still incomplete.

## One registry and one migration runner

Use a small strict-C89 array of records containing version, compiled SQL bytes,
length and SHA-256. Keep it with `sqlite_storage.c` or in a generated checked-in
header included there. The generator must support `--check`, and every build
must verify that the header matches the reviewed lock and SQL files. Runtime
execution must use compiled SQL only, never bytes read from an artifact.

The smallest compatible payload is the exact concatenation of migration 001
and migration 002, without inserted separators. The existing 56-byte SQL
footer already describes arbitrary payload bytes, their size and SHA-256;
its `v1` denotes the envelope format, not SQLite's `user_version`. There is no
need for JSON parsing, compression or another runtime dependency. Registry
lengths identify every constituent migration, and exact byte comparison plus
the bundle checksum rejects missing, reordered or appended SQL.

The new runner may recognize both the exact historical v1 payload and the
current v2 bundle. The recognized payload explicitly selects the target:

| Database | Historical v1 payload | Current v2 bundle |
| --- | --- | --- |
| Empty | Create v1 | Create v1 and apply v2 atomically |
| Valid v1 | Open without schema change | Upgrade atomically to v2 |
| Valid v2 | Reject downgrade | Open without schema change |
| Unknown/newer/inconsistent history | Reject | Reject |

This preserves existing v1 fixtures and old artifact behavior without quietly
upgrading a caller that supplied only v1 bytes. The current desktop and server
artifacts always embed the current bundle. Old Wena executables will reject
a v2 database; reverse migration is not promised.

Expose one shared read-only schema validation function for open, backup and
restore. It must check that `user_version` is supported and every migration
row is exactly the contiguous range 1..user_version, with INTEGER versions,
TEXT checksums, correct byte lengths/content, and no extra future rows. Reject
an empty or forged history rather than checking only `WHERE version=1`.
Quick/integrity/FK checks remain separate existing functions. Metadata alone
does not prove the physical schema; validate required tables/columns and
constraints using fixed registry-owned queries before migration and startup.

For startup, validate the executable payload before opening/creating a path.
Acquire `BEGIN IMMEDIATE` before reading the migration version/history that
governs an upgrade, then recheck them under that lock. This prevents two
concurrent openers from both acting on an earlier version. Execute all missing
compiled SQL and bind all metadata rows in the same transaction; set
`user_version` only after successful migration and foreign-key validation.
A failed DDL statement, metadata insert, busy lock, disk write or commit must
leave the previous schema and data intact and return no usable handle.

For restore, first validate sidecar/hash/integrity/history without writing the
backup. Copy it into the existing private staging workflow, open and upgrade
that copy to the requested compiled target, checkpoint/close it, and verify
integrity before stopping the listener. Keep the source backup and current
database unchanged on a failed staged upgrade. Preserve the existing recoverable
old generation and lifecycle rollback. Do not defer schema migration until the
new file has replaced the live database. The existing restore path's unrelated
file-publication durability gaps must not be described as solved by schema work.

## Description semantics and bounded first implementation

The pinned [card schema](https://github.com/wekan/wekan/blob/689a393841f08c3a020a4ef435b869b7641b21df/models/cards.js)
defines `description` as optional String with default `''`; its schema does not
declare a length limit. Ordinary card `setDescription` writes the supplied
value; linked cards and linked boards target their underlying object instead.
`getDescription` returns null for absent/empty text. The native slice has no
linked-card model, so it must explicitly cover ordinary local cards only.

The pinned [description form](https://github.com/wekan/wekan/blob/689a393841f08c3a020a4ef435b869b7641b21df/client/components/cards/cardDetails.js)
sends textarea text directly to `updateAccessibleCardContent`. Its draft
comparison strips whitespace to avoid phantom unsaved drafts, while comments
explicitly describe the stored Markdown text as raw. Do not copy draft
comparison normalization into persistence: two trailing spaces before LF are
a Markdown hard break and must survive Save/reopen.

Implemented migration 002 adds `card_descriptions` with `card_id TEXT NOT NULL
PRIMARY KEY`, `board_id TEXT NOT NULL` and `description TEXT NOT NULL DEFAULT ''`.
A composite foreign key references the exact cards(board_id,id) tuple, backed by
a new unique index. The description check requires TEXT, at most 1024 UTF-8 bytes
when encoded correctly, and no embedded NUL. The native mutation adapter must
validate UTF-8 and disallowed controls before writing; SQLite itself does not
validate UTF-8 encoding. Missing description rows represent empty text. The
parent card version remains the optimistic guard, and existing card columns,
IDs, positions, versions and positional INSERT statements remain unchanged.

The 1024-byte limit is an explicit current versioned storage bound, chosen to
fit native editing and the existing encoded command body. Increasing it requires
a reviewed schema migration and matching API/form-capacity work. Preserve exact
accepted text or reject the operation; never truncate. This is not unbounded
WeKan import/export parity.

Use a separate on-demand `WenaCardDescription` value with a 1025-byte buffer and
authoritative card version. Do not append a large description buffer to every
one of the 2048 card snapshot entries. The loader queries exact board/card
scope, validates SQLite TEXT type, embedded NUL, UTF-8 and capacity, then
publishes output only after the complete read succeeds.

For the first native editor, support 0..1024 UTF-8 bytes, preserving CR, LF, TAB,
spaces and Markdown literally. Reject NUL, other C0 controls, DEL, C1 controls,
malformed UTF-8 and over-capacity values. Empty text is a valid clear operation.
This is an explicit local limitation, not full WeKan text-size parity. A
1024-byte value needs at most 3072 bytes when percent encoded and fits the
existing 4097-byte domain body with scope/version fields. A 4096-byte value
does not; do not silently truncate or enlarge every command before auditing
HTTP/region/input limits.

Reuse `WenaCardMutation` and add typed `EDIT_CARD_DESCRIPTION` dispatch. Extend
the existing form decoder with a description-specific empty/multiline policy;
do not weaken title, ID or numeric decoding. Validate UTF-8 explicitly before
SQL, since a successful response need not echo the description. Bind text and
guard actor, board, card, active state and expected card version. Description
and title edits share the same row version so competing edits conflict.
Commit the response checksum/idempotency row with the description write using
the current transaction boundary. Native replay behavior remains rejection
without a second mutation. Publish the detail cache only after commit.

Use bounded Nuklear multiline input with a one-byte overflow sentinel, existing
Save/Cancel/Close controls, stale-selection checks, persistent failure text,
and a read-only plain-text description view. Markdown rendering, autosaved
drafts, linked-card authorization/history and arbitrary-length import/export
are later explicit work; none should be marked complete by this slice.

## Checklist schema and feature slice

The pinned [checklist](https://github.com/wekan/wekan/blob/689a393841f08c3a020a4ef435b869b7641b21df/models/checklists.js)
and [item](https://github.com/wekan/wekan/blob/689a393841f08c3a020a4ef435b869b7641b21df/models/checklistItems.js)
models are separate collections. Items reference both checklist and card;
board IDs are denormalized. Native relational constraints should make that
scope consistency mandatory. Do not store checklist JSON in the description.

| WeKan field | Proposed relational value | Semantics to preserve |
| --- | --- | --- |
| Checklist `_id`, `cardId`, `boardId` | Stable IDs and scoped foreign key | A checklist belongs to exactly one card and its board |
| Checklist `title` | TEXT | WeKan default is `Checklist`; native creation can show the canonical localized default |
| Checklist/item `sort` | Native INTEGER position | Deterministic local order; future import must convert fractional WeKan sort values explicitly |
| Checklist `hideCheckedChecklistItems`, `hideAllChecklistItems` | Nullable INTEGER booleans | Preserve absence if round-trip parity is required |
| Checklist `showChecklistAtMinicard` | Nullable INTEGER boolean | NULL follows the board default; false must not become NULL |
| Checklist `finishedAt`, `createdAt`, `modifiedAt` | Nullable INTEGER UTC milliseconds | Preserve optional data; update times atomically when the corresponding behavior is implemented |
| Item `_id`, `checklistId`, `cardId`, `boardId` | Stable IDs and composite scoped FK | Item/card/board cannot disagree with checklist scope |
| Item `title`, `isFinished` | TEXT, INTEGER 0/1 | Item default is unfinished; check and uncheck are explicit guarded writes |
| Item `createdAt`, `modifiedAt` | Nullable INTEGER UTC milliseconds | Same storage convention as checklist timestamps |

Add `UNIQUE(board_id,id)` for cards, then a checklist FK to cards(board_id,id)
and `UNIQUE(board_id,card_id,id)` on checklists. An item's composite FK targets
that exact checklist tuple. Use `ON DELETE RESTRICT`, explicit transactional
child deletion and unique scoped position indexes. Every mutable checklist
and item gets its own positive optimistic version. Shared operation names and
ID generation must include entity kind to avoid create-request collisions.

The first complete UI slice should create/rename/delete checklists and items,
toggle item completion, display ordered items/progress, and preserve state on
reopen. A card-details loader can bound 32 checklists and 256 items per opened
card and fail atomically on overflow; these are native display limits, not
proof of WeKan import parity. Use existing form/title validation, transaction
and mutation conventions. Empty-checklist progress is 0%, matching WeKan;
completion requires nonempty all-finished items unless a later visibility
feature explicitly implements upstream `hideAllChecklistItems` behavior.

Minicard tri-state visibility, per-checklist filtering, cross-card moves,
activity/history emission and full import/export should remain separate open
items until each has implementation and regression evidence.

## Required tests before marking a slice complete

- Migration: byte-identical v1 golden; fresh v1 and fresh v2; populated v1-to-v2
  preserving IDs/data/order/version/idempotency; second-open no-op; no downgrade;
  v3 rejection; missing, extra, reordered, malformed-type or wrong-hash metadata;
  forged physical schema; altered bundle/lock/header/SQL/footer; corrupt files.
- Atomic migration: failure at second migration statement, metadata insert and
  commit; concurrent openers; process termination before/after DDL and commit;
  reopen shows either complete v1 or complete v2, never a partial schema.
- Backup/restore: both supported versions; v1 backup staged upgrade under v2;
  unchanged source backup/hash; staged migration failure never stops listener;
  wrong checksum/newer schema rejection; lifecycle restart failure restores
  previous database; backed-up v2 description survives reopen.
- Description: empty/default/clear, ASCII and multibyte success, exact 1024-byte
  boundary, 1025-byte overflow, newline/tab/CRLF and Markdown spaces, malformed
  percent input, duplicate/missing field, NUL/control/invalid UTF-8, oversized
  SQLite content, wrong actor/board/card, archived card, stale row version,
  competing title edit, replay, late rollback and persistence after reopen.
- Native description UI: actual Nuklear text input, overflow detection, Save,
  Cancel/Close unchanged state, stale card selection, retained failure input and
  clipped multiline display. Consume draw commands on every simulated frame.
- Checklist slice: same scope/version/replay/rollback matrix; composite FK
  violations; create/rename/delete/toggle; empty/all/partial completion counts;
  stable ordering, capacity overflow, exact nullable tri-state preservation,
  concurrent append, real UI interaction and reopen after every mutation kind.
- Run current title/create/move/archive/restore/hierarchy suites unchanged to
  demonstrate that field-specific multiline support did not weaken their
  controls. Run the full native suite and relevant ASan/UBSan groups after
  artifact, schema, runtime and restore integration.
