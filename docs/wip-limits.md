# Work-in-progress limits

`models/wip_limit.[ch]` contains shared, allocation-free C89 arithmetic for list
limits and future lane/group limits. It has no database or UI dependencies.
Guarded setting writes are implemented. Snapshot loading, card mutation
enforcement and native UI integration remain open roadmap steps; stored limits
do not yet restrict card actions in the application.

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
