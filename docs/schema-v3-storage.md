# Schema-v3 checklist storage

Migration `003_checklists.sql` adds `checklists` and `checklist_items` to the
existing compiled migration registry. Versions 1 and 2 retain their exact
published bytes and checksums. Current artifacts carry the concatenated v1-v3
bundle; historical v1/v2 bundles still select their original target and reject
downgrades from v3. No second migration framework or runtime dependency is added.

This slice implements schema, verification and migration/restore tests. Native
checklist mutations, UI and complete WeKan import/export are separate work. The
model/adapter contract is described in `docs/checklists-design.md` in the main
integration workspace and was checked against pinned WeKan
`689a393841f08c3a020a4ef435b869b7641b21df`.

| Stored field | Contract |
| --- | --- |
| IDs | Explicit TEXT NOT NULL primary key; native ID length 1–64 bytes, no NUL |
| Board/card scope | Required; checklist FK references cards(board_id,id) |
| Item parent scope | Composite FK references checklist(board_id,card_id,id) |
| Title | TEXT, 1–128 bytes, no NUL; adapters additionally validate UTF-8 and controls |
| Position | INTEGER, 0–2147483647; unique per card for checklists and per checklist for items |
| Display flags | Independent INTEGER 0/1, default 0 |
| Minicard override | NULL inherits; explicit INTEGER 0 hides, 1 shows |
| Item completion | INTEGER 0/1, default 0; counts/progress are derived, never stored twice |
| Version | Positive INTEGER, default 1 |
| created_at / updated_at | Nonnegative INTEGER UTC epoch milliseconds, default 0; values are retained exactly |

The card version is the aggregate optimistic boundary for every checklist
mutation. Changed checklists/items also increment their own versions; untouched
rows do not. Mutation adapters perform those changes together in the existing
guarded transaction. This schema does not introduce triggers that silently alter
card versions or timestamps. Default zero timestamps represent the Unix epoch;
normal creation/edit adapters must bind the appropriate timestamp explicitly.

Foreign keys use DELETE RESTRICT. There is no deletion API in this slice, and
archiving a card preserves its checklist and item rows. Future deletion or
cross-card/board movement requires its own complete transaction and scope tests.
The byte, position and collection limits are current native bounds, not full
WeKan round-trip limits; accepted values must be preserved or rejected whole.

The compiled schema-object registry validates the exact new table definitions,
including unique, foreign-key, type, bound and NULL constraints. Both open and
backup/restore reject altered tables/views and inconsistent migration history.
The generator additionally pins the immutable v2 description migration hash.

`tests/test_sqlite_schema_v3.sh` covers blank creation, populated v1/v2-to-v3
upgrades, repeated opens, old target rejection, default and explicit flags,
millisecond timestamp retention, 43 constraint failures, archive retention and
v3 backup/restore. Old backups are upgraded in a private verified copy before
the listener stops; staging failure leaves the live database and source backup
unchanged, and restart failure restores the previous generation.

Injected late DDL/history/commit errors roll back. Process termination at those
same stages reopens complete v2; termination after successful commit retains
complete v3. Four concurrent v2 upgraders produce exactly three history rows.
The existing v2 suite remains pinned to its historical bundle prefix and runs
alongside these tests. Publication fsync/directory guarantees and a full v1
physical schema fingerprint remain outside this schema slice.

Local validation on 2026-09-07 against the pinned SQLite 3.51.3 build: all
**70 isolated native suites passed**, zero failures or skips. The new v3 suite
also passed **ASan/UBSan**, with LeakSanitizer disabled for this container. Strict
C89 compilation and migration lock/header verification passed. Peer review found
no additional DDL defect. Main-worktree checklist UI/backend and connection
hardening integration require their own combined test run.
