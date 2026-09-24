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

This step provides storage only. Individual card timestamp writes, guarded lane
cascades, snapshot loading, destination checks, native cache publication and the
shared menu/archive-browser integration remain open roadmap items. Cascade tests
must distinguish pre-archived cards from cards archived by the lane operation,
including timestamp ties, repeated archive/restore, rollback and WIP limits.
