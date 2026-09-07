# Schema-v2 storage validation

Validated locally on 2026-09-07 in an isolated worktree based on `0a61868`
plus migration-registry prerequisite `efa0311`. This report concerns the storage
and artifact slice; later native description mutation/UI work is separate.

- All **69 native suites passed**, with zero failures or skips, including the
  new schema-v2 suite, migration embedding, runtime startup, desktop startup,
  existing v1 storage/backup/restore and the preexisting card/hierarchy suites.
- Four targeted **ASan/UBSan** suites passed: schema-v2, SQLite storage, backup
  and restore. LeakSanitizer was disabled for this container.
- Strict C89 compilation uses `-std=c89 -pedantic-errors -Wall -Wextra -Werror`.
  The dynamic test dependency is the pinned local SQLite 3.51.3 build.
- The published v1 SQL SHA-256 remains
  `e4760a2b70d6651ee84dce93642ccdd4ce8991b488dece5d231e66053f065da5`.

The v2 suite verifies fresh creation, populated v1 upgrade, exact preserved
domain rows/versions/idempotency, repeated opens, downgrade refusal, ordered
bundle verification, self-consistent but untrusted artifact rejection, typed
history corruption and missing/altered physical v2 definitions. The new table
rejects NULL card keys, wrong card/board scope, NUL text and over-1024-byte text;
valid multiline UTF-8, empty text and Markdown whitespace survive reopening.

Injected failures at late DDL, history insertion and commit roll back. Separate
process-death tests terminate during those three upgrade stages and reopen a
complete original v1 database; death after successful commit retains complete
v2. Four simultaneous v1 upgraders complete with exactly two history rows.

Restore verifies the copied bytes and upgrades a staged v1 backup before stopping
the listener. Tests prove failed staging leaves the listener, live database and
source backup unchanged; failed restart restores the previous live generation.
Current backups preserve descriptions. Malformed schema backups with recomputed
valid file sidecars are rejected before listener stop.

Peer review identified two defects before integration: SQLite TEXT primary keys
needed explicit NOT NULL, and metadata-only validation missed missing or altered
new tables/indexes. Both were fixed and given regression tests. No complete
historical v1 schema fingerprint, unbounded WeKan text parity, cross-platform GUI
release validation or complete restore-publication power-loss durability is
claimed by this slice.
