# Schema-v6 board settings

Migration `006_board_settings.sql` adds one optional `board_settings` row per
board. Its `show_checklist_count` flag stores the native equivalent of WeKan's
`allowsChecklistCountBadgeOnMinicard`, an opt-in setting with canonical default
false. The mapping is grounded in `models/boards.js` and
`client/components/cards/minicard.js` at pinned WeKan
`689a393841f08c3a020a4ef435b869b7641b21df`.

| Field | Stored contract |
| --- | --- |
| `board_id` | TEXT NOT NULL primary key, 1–64 bytes without NUL |
| `show_checklist_count` | INTEGER NOT NULL, exactly 0 or 1, default 0 |
| Ownership | Foreign key to `boards(id)` with `ON DELETE RESTRICT` |

A missing row means false. Upgrades do not manufacture rows for existing boards
and do not enable the badge. The adapter may store an explicit false after a
previous true value; reading either absence or explicit zero yields the same
canonical setting. A false request for an absent row is a guarded no-op. Native
identifier validation remains stricter than the storage byte/type constraints.
SQLite affinity may convert an input before the stored-type check executes.

The existing `boards.version` is the optimistic boundary. This table does not
add a second version, cached checklist counts, timestamps or triggers that
silently change the board. The adapter updates the flag and board version
through the existing guarded transaction. The primary key provides indexed
single-board lookup; no additional index is needed.

## Architecture decision

Use an additive extension table instead of `ALTER TABLE boards`. The existing
compiled migration registry already verifies exact CREATE TABLE/INDEX programs
and objects. Extending it to recognize ALTER would add another class of schema
transformations and require new physical-schema reconstruction rules. It would
also change the original three-column board contract relied upon by older
fixtures and legacy-target code. A board-owned extension table, as already used
for card descriptions, preserves that contract while remaining a single source
for the new setting. It introduces no runtime dependency or parallel migration
architecture.

Published v1–v5 SQL remains byte-identical. The verifier now explicitly pins the
published v5 label migration alongside v1–v4. Current artifacts identify the
concatenated v1–v6 bundle; historical prefixes select their original target and
reject downgrades. The schema runner, connection hardening and staged restore
implementation need no code changes.

`tests/test_sqlite_schema_v6.sh` exercises fresh creation and populated upgrades
from every v1–v5 target. Cards, descriptions, checklists, completed items, exact
label colors/assignments, domain versions and idempotency records survive.
The setting is initially absent for every upgraded board. Tests cover explicit
false/true and default values, 64-byte IDs, 14 type/ownership/duplicate failures,
repeat opens and 17 malformed history/table fixtures rejected by open, schema
validation, backup and restore.

Injected table-creation, history-write and commit failures leave complete v5.
Process termination at those stages also reopens complete v5; termination after
commit retains complete v6. Four simultaneous v5 upgraders leave exactly six
migration-history rows. Old backups upgrade in a private verified copy before
the listener stops; staging failure preserves the live database, restart failure
rolls back to the prior generation, and backup source bytes are unchanged.
Current v6 backup/restore retains the explicit settings and existing board data.

This file records the schema slice. Setting controls, guarded adapter behavior,
compact count projection and real desktop presentation require their own main
integration tests. Full expandable minicard checklist presentation and complete
WeKan board settings parity remain separate work.

Validation on SQLite 3.51.3: all 102 existing suites passed, zero failed or
skipped, in the isolated schema worktree. The new v6 suite passed strict C89
and ASan/UBSan, with LeakSanitizer disabled for this container. The historical
v5 suite also passes against its unchanged prefix in the v6 registry. These
results do not replace main-worktree UI/backend integration gates.
