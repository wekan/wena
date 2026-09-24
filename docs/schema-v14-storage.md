# Schema v14: board members and card people

Migration `014_card_people.sql` is additive. Existing boards, actors, cards and
metadata remain unchanged, and both new tables start empty. Upgrading does not
grant membership or infer it from the actor opening a workspace.

`board_members` records a unique `(board_id, actor_id)` relationship, active flag,
revision and creation/update timestamps (update time cannot precede creation). The active flag describes
assignment eligibility; it is not a grant of administrator or write permission.
Both parents must exist. The actor index supports finding an actor's board roster
entries without scanning every board.

`card_people` stores ordered sets using one schema for the exact field names
`members` and `assignees`. Assignments are unique by board, card, field and actor;
positions are unique within each board/card/field. The card's aggregate revision
will guard assignment changes, as it does label assignments. The actor and
complete board/card scope are enforced by foreign keys. There is deliberately no
foreign key to `board_members`: a departed member's assignment remains removable,
and deleting a roster entry must not silently alter card contents.

Native IDs are 1–64 ASCII letters, digits, underscores or hyphens. Boolean flags,
positive revisions, nonnegative timestamps and positions are checked after
SQLite's normal column affinity conversion. Positions range from 0 through
2147483647. The shared person model bounds complete rosters and card sets to 2048
IDs; transaction readers and writers must enforce that bound before publishing
or changing records. The schema itself does not impose a global actor limit.

The existing migration runner owns the transaction and verifies the exact table
and index DDL. The shared schema test harness exercises every v1–v13 upgrade,
constraints and defaults, preserved parent revisions, assignment retention after
roster removal, schema-object removal, seven injected DDL/ledger/commit failures,
retry and reopening. Existing migration bytes and checksums remain unchanged.

`server/card_people_store` supplies common strict readers for roster rows and both
card fields within a caller-owned read or write transaction. They preserve original
positions, return board/card and roster/actor revisions, reject malformed or
incomplete data, and publish output only after every row validates. They never
commit, roll back or grant access. Native callers must still stage these outputs
until their enclosing read transaction commits.

`server/mutations/card_people` provides the shared single-row assignment primitive
for either field. It revalidates the caller's exact captures, checks active card
parents, applies the shared person-set model, retains order gaps and verifies
unchanged roster/card metadata and the other field after writing. Only changed
cards advance revisions. It neither advances the board revision nor commits:
the enclosing domain operation must authorize the caller, guard replay, verify
the whole batch, advance the board once and commit or roll everything back.

Native operation `WENA_DOMAIN_SET_SELECTED_PERSON` accepts an exact typed span of
1–2048 unique card IDs/revisions, including a one-card span. Its form supplies
`personId`, `field` (`members` or `assignees`), `enabled` (`0` or `1`) and
`expectedBoardVersion`. The existing guarded persistence transaction checks the
actor and replay identity. The operation preflights every selected card before
writes, advances only changed cards and advances the board once. All-no-op
requests preserve revisions and do not consume a mutation identity. Complete
roster and per-card fingerprints are checked after writes, including both person
fields, ordering, card metadata and every selected no-op card. Fingerprints use
length-framed strings and fixed-width numbers, not native structure bytes. The
HTTP dispatcher does not expose this typed native operation.

Roster management, transactional transfer filtering, native capture adapters and
paginated person controls remain pending in `ROADMAP.md`. No native UI exposes
assignment writes yet. As with the existing native domain adapter, the caller is
responsible for authentication and board authorization before invoking it.
