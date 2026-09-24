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

Guarded assignment writes, roster management, transactional transfer filtering,
native snapshots and paginated person controls remain pending in `ROADMAP.md`.
Until those operations are implemented, these tables have no automatic writers.
