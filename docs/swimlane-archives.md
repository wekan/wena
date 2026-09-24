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

Typed local swimlane archive/restore operations now use the common guarded
transaction, with exact scope/revision, replay protection and no-op handling.
The lane reader shares list archive validation and preserves failed outputs.
The cascade captures all scoped cards before writes, bounded by the same 2048
card capacity as native ordering. It rejects excess capacity without partial
changes. Archive chooses a lane timestamp strictly after every existing card
timestamp, then uses that floor when archiving active cards. This prevents a
pre-archived card from sharing the new lane timestamp, even within one clock
tick or with a future stored time.

Restore selects archived cards at/after the lane's known archive time; missing
times select none. It reuses the card writer and checks each restored card
against the shared WIP rule. If any restoration exceeds a hard limit, the
complete transaction rolls back; soft limits allow restoration. Ordinary lists
and their revisions are retained, including already-archived lists. A final
read verifies lane state and the full captured card set, revisions, archive
flags and timestamps before commit. No HTTP route is added.

File-backed tests cover pre-archived cards, timestamp separation, repeated
archive/restore, empty lanes, unknown legacy times, stale/scope/replay failures,
ignored writes, altered/relocated cards, a second-card failure, late rollback,
multi-card WIP rejection/retry, excess capacity, corruption and reopening.
Atomic board snapshots now load list and swimlane archive flags through the same
bounded metadata loader as colors and WIP settings. It validates table presence,
IDs, exact scope, parents, duplicate rows, flags and timestamps, including
orphaned metadata claiming the board. Legacy missing tables retain active
defaults; missing modern tables or replacement views fail without changing
the prior snapshot. Child cards remain in the snapshot with their archive flags.
Concurrent WAL tests change lane/card flags together between reader statements
and verify that snapshots never combine their before and after states.
Destination checks, native cascade publication and the shared menu/archive
browser integration remain open roadmap items.
