# Native checklist model, storage and UI contract

Status: C89 models, guarded native SQLite operations and the native checklist UI
are integrated with schema-v4. Adapter fixtures retain exact immutable v3 table
bytes, while the current bundle adds the verified v4 query index. No checklist
HTTP mutation route is exposed. Complete remaining parity limits are listed below.

## Source provenance

Behavior was checked against the local pinned WeKan revision
`689a393841f08c3a020a4ef435b869b7641b21df`:

| Upstream source | SHA-256 |
| --- | --- |
| [models/checklists.js](https://github.com/wekan/wekan/blob/689a393841f08c3a020a4ef435b869b7641b21df/models/checklists.js) | `c190e5d9f97c8f97650d14aa30188311d938b0750cf6c62b7f785aee7ac0f98b` |
| [models/checklistItems.js](https://github.com/wekan/wekan/blob/689a393841f08c3a020a4ef435b869b7641b21df/models/checklistItems.js) | `32e843648c80245f13270b6e3417f4f460f8871ee87d23f7216d7cdc254e0c0f` |
| [models/lib/minicardChecklistVisibility.js](https://github.com/wekan/wekan/blob/689a393841f08c3a020a4ef435b869b7641b21df/models/lib/minicardChecklistVisibility.js) | `c36b06ea61579903b25b7a665872e7e2aa5b0ca6ac16596b8916f99560dcd772` |
| [models/lib/checklistItemTitles.js](https://github.com/wekan/wekan/blob/689a393841f08c3a020a4ef435b869b7641b21df/models/lib/checklistItemTitles.js) | `b0588ffc721b9d576f08c124ce34e3bc8449846ee1e9c075d24211b160d25fe8` |

These sources establish behavior; the native implementation uses the existing Wena
model validators and contains no copied JavaScript implementation.

## Model contract

| WeKan field or behavior | Native representation |
| --- | --- |
| `_id`, `cardId`, optional denormalized `boardId` | Required stable IDs, validated against the actual card parent; missing upstream board scope must be derived from that parent |
| Item `checklistId`, `cardId`, optional `boardId` | All required; the item parent check compares all three relationships |
| `title` | Nonempty strict UTF-8, at most 128 bytes, preserving spaces; no C0, DEL, or C1 controls |
| Numeric `sort` | Native integer position from 0 through 2147483647; deterministic comparator uses position then stable ID |
| Item `isFinished` | Strict boolean, initialized explicitly; checklist counts are derived from items |
| `hideCheckedChecklistItems`, `hideAllChecklistItems` | Independent strict boolean display flags |
| Optional `showChecklistAtMinicard` | Inherit, explicit hide, or explicit show; explicit false overrides a true board default |

The title and collection bounds are current Wena safety limits, not claims about
upstream schema limits. Upstream fractional or negative `sort` values cannot be
passed directly as native positions. A future import adapter must explicitly map
the complete upstream ordering to native ordinals, with stable ID as a tie-break,
and preserve source values separately if lossless round-trip export is required.
Never truncate titles or collections silently.

Constructors validate detached rows. Adapters must additionally call the parent
validators before publishing them. Archived parent cards retain their checklist
data; active-state authorization belongs to mutation adapters. Invalid constructors
clear their destination, matching existing model conventions. Other failed
operations preserve their output or existing item.

`wena_checklist_progress` accepts a complete collection for one checklist, including
hidden items, and rejects foreign scope, duplicate IDs, malformed rows, or more
than 1024 items. It derives total, finished, rounded integer percentage, and actual
completion. An empty checklist is 0 percent and not actually complete. The separate
`is_finished` result follows WeKan's display helper: hide-all also makes that helper
true, even when items remain incomplete. Hiding items never changes item flags or
counts. Percentage rounding matches positive `Math.round` (1/8 is 13 percent).

The model constructor is separate from the canonical item-entry parser in
`models/checklist_item_titles.[ch]`. The native single-item entry applies trimming;
the tested optional newline splitting/order helper is not yet an atomic batch
mutation. Local timestamps are represented by the adapter below. Source activity,
copy/move effects and persisted board-default semantics remain open.

## Implemented native adapter contract

`client/features/checklist_mutation.[ch]` loads a complete heap snapshot for one
active card, bounded to 64 checklists and 1024 items **across that card**, with no
truncation. Models, scope, row types, timestamp validity, ordering, and versions
are validated within one consistent read transaction. The neutral
`checklist_store.[ch]` snapshot validator is shared with UI callback validation.
A failed load leaves the caller's snapshot untouched. Archived cards retain their
data but this editable snapshot loader rejects them.

Supported intents are create/rename checklist, set checklist display flags, and
add/rename/set-finished item, and permanent item/checklist deletion.
They reuse the existing typed SQLite transaction and durable idempotency adapter;
there is no second mutation transaction architecture or direct HTTP mapping.
Every change checks actor, board, active card, aggregate expected card version,
and applicable expected checklist/item versions. Every real change increments
card.version; checklist rename/display changes increment its own version, item creation increments
its parent checklist version (as does item deletion), and item rename/toggle increments only its own row
version. Unrelated rows remain unchanged. Identical title/completion/display saves are
fully guarded no-ops with no version, timestamp, or idempotency writes. Committed
request replay rejects, including after reopening.

New IDs derive from operation, actor, route and durable request number. Creates
append within the exact parent and enforce total card capacities inside the
transaction. Local timestamps are epoch milliseconds from SQLite's portable
epoch-second clock (second precision); updates use max(previous, current) and
never invent source timestamps. The snapshot does not own a mutable GUI cache:
callers reload after commit and report a reload failure separately from a save
failure. A failed mutation writes neither rows nor request metadata.

`tests/test_checklist_mutation.sh` exercises missing schema, exact title decoding,
invalid input, scopes, aggregate/row staleness, no-ops, actor ID separation,
concurrent card changes, rollback after child writes, timestamp monotonicity,
reopening, malformed snapshots and both collection capacities. Display coverage
includes all twelve hide-checked/hide-all/minicard combinations, explicit false
versus inherited NULL, and unchanged actual completion/counts. The fixture
`tests/fixtures/checklist_schema_v3.sql` is copied exactly from the schema agent's
reviewed additive migration; it does not alter the application migration chain.

Reorder, cross-card movement,
activity emission, batch item insertion and lossless WeKan import/export remain
unimplemented.

## Permanent deletion contract

The pinned [accessible checklist operations](https://github.com/wekan/wekan/blob/689a393841f08c3a020a4ef435b869b7641b21df/server/lib/accessibleChecklistOperations.js)
remove exact-scope children before their checklist and remove individual items by
item/checklist/card/board scope. Source SHA-256 is
`5b30f6fb2fda529a7c93d5c73dbd388f4d718aabb0cfcd65c68828e9cbce5156`.
The older upstream REST path directly removes a checklist; the native adapter
follows the child-first accessible path, avoiding its documented orphan problem.
Upstream item removal also cleans activities; native activity parity is not claimed.

Native deletion is explicitly permanent, with no undelete or audit-retention API.
Item deletion checks card/checklist/item versions, deletes only that exact item,
then increments parent checklist.version/updated_at and card.version. Checklist
deletion checks card/checklist versions, removes only exact-scope children, verifies
none remain, deletes the parent, and increments only card.version. The aggregate
card guard rejects any intervening child edit. No other checklist/item version or
position changes; gaps remain valid and later creation appends after the maximum.
The existing RESTRICT foreign keys are retained unchanged.

Missing, stale, archived-parent, wrong-scope, unknown-actor and committed replay
requests reject. Trigger/metadata/commit failure rolls back child deletions, parent
changes, aggregate version and request metadata together. The adapter checks for
foreign-scope children and survivors even when an external caller disabled SQLite
foreign keys. `tests/test_checklist_delete.sh` covers these boundaries, empty
checklist deletion, gap append, unrelated rows and database reopening. UI
confirmation is required before invoking these destructive callbacks.

## Schema-v3 design

Use additive `checklists` and `checklist_items` tables after the verified v2 chain.
Both need explicit `TEXT NOT NULL PRIMARY KEY` IDs and bounded, typed title and
position constraints. Require native board/card scope instead of optional
upstream denormalized fields.

- `checklists`: board_id, card_id, title, position, display flags, nullable minicard
  override, and optimistic version. Reference existing `cards(board_id,id)` through
  `(board_id,card_id)`. Add `UNIQUE(board_id,card_id,id)` for the child composite
  foreign key and `UNIQUE(card_id,position)` for native order.
- `checklist_items`: board_id, card_id, checklist_id, title, position, is_finished,
  and optimistic version. Reference checklist `(board_id,card_id,id)` through all
  three parent columns. Add `UNIQUE(checklist_id,position)`. Store strict boolean
  completion per item; do not persist a second source of truth for counts.
- Schema constraints must check integer types and bounds, title byte length and
  NUL exclusion, boolean domains, and minicard NULL/0/1. Native adapters additionally
  enforce the shared UTF-8/control validation and bounded snapshot capacities.

Mutations should reuse the current typed operation and guarded SQLite transaction
adapter: authenticated actor, route board, exact card/checklist scope, expected
version, durable request identity, and cache publication only after commit. The implemented propagation contract above applies to create, edit and toggle.
Specify equivalent contracts before introducing reorder or cross-card movement. Reordering requires a complete
scope/order snapshot and atomic collision-safe positions; checklist movement must
update every child's denormalized scope in the same transaction.

Before claiming complete checklist parity, specify full source timestamp/provenance retention,
verify migration and restore registries, and test rollback, replay, stale parent
and item snapshots, cross-scope inputs, capacity, reopening, and derived counts.
Do not expose an HTTP route or claim WeKan round-trip compatibility before those
adapters and activity/authorization semantics are reviewed and tested.

## Native confirmation text and display limits

Canonical confirmation wording comes from
[client/components/cards/checklists.jade](https://github.com/wekan/wekan/blob/689a393841f08c3a020a4ef435b869b7641b21df/client/components/cards/checklists.jade),
SHA-256 `21578b285d102cfc0ea06f54500c90cdb8ea7b365aebe2659305e5f09710a832`.
The two upstream popup templates use their matching confirmation body and an
explicit `delete` button. The native contracts retain exact catalog wording:

| Use | Canonical key | English text |
| --- | --- | --- |
| Checklist heading | `checklistDeletePopup-title` | Delete Checklist? |
| Item heading | `checklistItemDeletePopup-title` | Delete Checklist Item? |
| Checklist question | `confirm-checklist-delete-popup` | Are you sure you want to delete the checklist? |
| Item question | `confirm-checklist-item-delete-popup` | Are you sure you want to delete the checklist item? |
| Confirm button | `delete` | Delete |
| Cancel button | `cancel` | Cancel |
| Child inclusion context | `r-with-items` | with items |

These keys contain no placeholders. Selected checklist/item titles are separate
plain text, never format strings. Whole-checklist confirmation should also show
the actual number of included child items beside the canonical child-context
label; completed and hidden children must be counted. This makes clear that the
operation includes children rather than merely removing a heading. The initial
native delete action is permanent and has no undo adapter. Its confirmation
requires the explicit Delete button; opening a confirmation or pressing Enter
must not execute deletion. Other entities' canonical no-undo warnings are not
reused because they make unrelated claims about attachments, activity history,
boards or accounts. No separately maintained translation wording is introduced.

The current native display-settings editor exposes hide-checked and hide-all
flags. The model/SQLite adapter also stores a nullable per-checklist minicard
visibility override and resolves inherited board defaults, but native card
canvases do not yet render checklist contents. An inactive minicard visibility
selector is therefore omitted from this editor; saving its visible settings
preserves the existing stored override. Model/persistence support alone does
not establish native minicard display parity. Canonical Default/Yes/No and
Show-on-Minicard contract keys may remain available for future implemented UI.

## Local cross-board transfer

The typed checklist and item transfer edits accept an optional `target_board_id`.
Omitting it preserves the same-board contract. An explicit empty, malformed,
missing or duplicate form `targetBoardId` is rejected. Both operations share
revision-guarded aggregate advancement and the existing transaction/idempotency
path. Whole-checklist transfer updates the parent and every child's board/card
scope; item transfer updates board/card/checklist in one statement. Destination
collections, capacities and postconditions are validated before commit.

Whole-checklist transfer uses SQLite's transaction-scoped
[`defer_foreign_keys`](https://www.sqlite.org/pragma.html#pragma_defer_foreign_keys)
for the existing composite foreign keys; commit/rollback resets it. No schema
change or foreign-key disabling is needed. The same fast regression bodies run
against same-board and cross-board fixtures via `test_checklist_cross_board.sh`.

This is trusted local persistence. WeKan's `server/models/checklists.js`
`moveChecklist` checks mutation rights on both source and destination boards and
moves activity references; its model hooks maintain denormalized board IDs.
Native remote authorization/activity adapters remain separate roadmap work.
The current native transfer form still presents same-board destinations; the
shared directory picker is being connected separately.
