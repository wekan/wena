# Work-in-progress limits

`models/wip_limit.[ch]` contains shared, allocation-free C89 arithmetic for list
limits and future lane/group limits. It has no database or UI dependencies.
Guarded setting writes, snapshot loading and card mutation enforcement are
implemented, together with the native editor, header warnings and Add card
availability. Combined list-group and swimlane limits are separate future work.

Schema v12 adds `list_wip_limits`, with one row per list and an exact board/list
foreign key. Missing rows will use defaults; stored rows default to value 1,
disabled and hard. Values are integers from 1 to 2147483647, permitting automatic
count adjustment beyond the editor's 99 maximum while staying representable on
32-bit native targets. Flags are integer booleans. A board/list index supports
snapshot loading. Parent list revisions will guard changes; this migration
does not rewrite parent or card rows or modify any earlier migration.

The shared migration harness verifies all v1–v11 upgrades, preserved card state,
parent revisions, archive timestamps and colors, constraints and exact settings
after reopening. Failures at table/index creation, migration bookkeeping and
commit leave no v12 objects and permit retry. Downgrades and missing tables are
rejected; the generated registry and embedding tests pin the complete bundle.

The typed local `EDIT_LIST_WIP` operation reuses the actor/board transaction,
optimistic list revision and durable request identity. Its `action` selects
value application, enabled toggle or soft toggle; the shared pure editor rule
produces the new state. No HTTP route is added. Strict readers check persisted
types before conversions, exact scope, flags, range and revision. Missing
settings use defaults; missing v12 tables and replacement views fail closed.
The active count spans the whole list and rejects malformed card archive flags
and wrong-board cards. Archived lists reject edits.

Changes advance only the list revision; same-value requests do not create
settings or reserve an identity. Post-write reads verify settings, revision,
list activity and unchanged card count before commit. Ignored writes, altered
settings, changed counts and late identity failures roll back. File-backed
tests also cover invalid/duplicate form fields, stale revisions, wrong scope,
unknown actors, read-only storage, replay, corruption, automatic count
adjustment above 99 and reopening.

Each native list now carries its WIP settings, defaulting to value 1, disabled,
hard. Snapshot loading shares the hierarchy metadata loader with list/swimlane
colors: one query per metadata kind, bounded by the loaded parents, with common
scope, duplicate, orphan and ID checks. Settings for archived lists are retained.
Pre-v12 databases use defaults; modern missing tables or views fail closed.
Invalid integer types/ranges and cross-board metadata leave the prior complete
snapshot unchanged. A concurrent WAL writer changes board and WIP data between
reader statements; tests verify each snapshot contains only one database state.

WIP callbacks reuse the native hierarchy adapter and its existing list selection
and request identity helpers. Loads read authoritative settings, active count and
revision inside one transaction; failures preserve all outputs. Edits publish
the exact result captured by the persistence transaction after commit, without
a second query or allocation. This includes no-op results and values raised to
the database count when the cached cards are stale. Result fields are cleared
for each persistence call, including failures and unrelated operations.
Tests cover actor/scope loss, stale versions, duplicate/archived models,
archived persisted lists, invalid values, replay, late rollback and byte-for-byte
cache equivalence with a fresh snapshot.

Card create, restore and move operations share one destination-limit check in
the existing write transaction. Create and restore validate the resulting count
before commit, rolling back the new active card if it exceeds a hard limit.
This covers creation with explicit parents and legacy automatic selection.
Moves check before append, insertion or reordering. Movement within a list,
including between swimlanes, adds no WIP; moving out remains possible when the
source is full or overfull. Archived cards do not count. Soft and disabled
limits permit increases, while malformed modern metadata fails closed.
Legacy schemas without the WIP table retain unlimited behavior.

File-backed tests cover full-list create/restore/append/insertion rejection,
same-list lane changes and reorder no-ops, movement out, an exact-limit create
after freeing capacity, archive/restore transitions, soft/disabled increases,
already-overfull lists, late rollback and retry. Failed operations preserve
cached rows, positions, card revisions and request identities. Restoration also
requires an active parent list, consistent with creation and movement.

The existing hierarchy editor opens the WIP form from the list menu, with the
same adapter context, selection checks and close/cancel handling. Canonical
WeKan translations label the menu and enabled/soft controls. The form shows the
authoritative active count and saved limit. Checking enabled/soft immediately
saves that toggle and closes on success; failed toggles preserve the previous
state. Numeric edits require Save and accept only decimal 1–99, with a bounded
draft that retains overflow for rejection. Enter in the field does not submit.
Cancel/Escape discard the numeric draft; failed writes keep it and its captured
revision. Real Nuklear/SQLite tests exercise these controls, automatic count
adjustment, invalid/over-limit values, late rollback/retry, stale revisions,
failed loads and persisted settings after reopening.

List headers count cached active cards across every swimlane before applying
the presentation filter; archived and foreign-board cards are excluded. Enabled
limits display count/limit through the shared colored-heading component, using
orange at the limit and red above it with its existing contrast calculation.
The same pure rule decides Add card availability. A full hard limit replaces
the Add card button with a non-interactive label while retaining List menu;
soft or disabled limits keep creation available. Malformed settings fail closed.
Rendering performs no database reads. Real mouse/draw-command tests cover these
states and changing cached archive flags, including fully filtered boards.

The port follows the original WeKan source:

- `client/components/lists/listBody.js`: hard limits use all active cards in a
  list, unfiltered and across swimlanes, before offering another card.
- `client/components/lists/listHeader.js`: reached means count >= limit;
  exceeded means count > limit. Explicit editor values are 1–99. Hard limits
  cannot be set below the current count, including when disabled. Enabling a
  limit or changing soft to hard raises a smaller saved value to the count;
  this automatic adjustment can exceed 99.
- `models/lists.js`: defaults are value 1, disabled, hard.
- `models/lib/wipLimitGroupDecision.js`: combined counts reuse the same
  reached/exceeded comparisons.

The public [WeKan API schema](https://wekan.github.io/api/v7.55/#listswiplimit)
also describes the value/enabled/soft fields. The checked-out source above is
the behavioral reference; no third-party implementation or dependency is added.

Callers supply the authoritative active count, number removed, and number added
within the scope. Creation/restoration adds one; movement inside the same list
removes and adds one. Wena allows non-increasing changes even when already over
a hard limit, so a reorder or move out cannot trap cards. Increasing changes may
reach a hard limit but may not exceed it. Soft limits report status and allow
the change. Disabled limits never warn or restrict. These rules do not replace
authorization, revision checks or database transactions.

Invalid flags, a zero limit, count underflow and projected-count overflow fail
without changing the output. Editor transitions likewise preserve failed
outputs and support updating a draft in place. The fast `wip-limit` suite covers
all small count/limit/mode/change combinations, every explicit 0–100 input for
counts 0–101, automatic transitions and maximum `size_t` boundaries.
