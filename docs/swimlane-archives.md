# Swimlane archives

The behavioral reference is WeKan's `models/swimlanes.js` archive/restore methods
and pure `models/lib/swimlaneArchive.js` helpers. Archiving an ordinary swimlane
archives its active cards, leaving already-archived cards and their timestamps
alone. Restoration uses card timestamps at or after the swimlane's archive time
to identify the cascade. Missing archive times do not select cards for restore.
Template swimlanes additionally cascade their lists; template models remain a
separate porting step.

Schema v13 adds `swimlane_archive_state` with a boolean flag and archive timestamp,
and `card_archive_state` with the timestamp accompanying the existing card flag.
Both have strict scoped foreign keys and board indexes. Timestamps are nonnegative
SQLite integers, with zero representing no known archive time. Missing rows
preserve legacy defaults; the migration inserts none and rewrites no existing
cards, revisions, list archives, colors or WIP settings. All v1–v12 migration
bytes remain immutable.

The shared migration harness covers every earlier schema prefix, defaults,
strict types and bounds, duplicate/orphan/cross-board rows, parent deletion
restrictions, downgrade rejection and reopening. Injected failures at each table,
each index, migration bookkeeping and commit leave no v13 objects and allow a
retry. Registry and embedding checks pin the complete schema bundle.

Individual card archive/restore now uses a shared strict state reader/writer
inside the existing actor/revision/request transaction. Archive stamps a positive
time greater than the prior timestamp, including re-archive within one clock tick
or when the stored time is ahead of the clock. Restore retains the timestamp;
restoring a legacy card without metadata does not invent one. A timestamp floor
allows a later lane cascade to stamp cards after its own archive time. Integer
overflow fails without a write. Pre-v13 databases retain flag-only behavior;
missing modern tables and replacement views fail closed.

Post-write validation verifies card revision, archive flag and exact timestamp
before commit. Failed reads preserve outputs, and native caches change only
after commit. File-backed tests cover scope/type corruption, ignored/altered
writes, identity rollback, repeated archive/restore, a future timestamp floor,
overflow, unchanged failed cache state and reopening. Existing legacy card
archive/restore suites continue to exercise their original v1 schema.

Guarded lane cascades, snapshot loading, destination checks, native cascade
publication and the shared menu/archive-browser integration remain open roadmap
items. Cascade tests must distinguish pre-archived cards from cards archived by
the lane operation, including timestamp ties, repeated archive/restore, rollback
and WIP limits.
