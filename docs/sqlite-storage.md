# SQLite storage and migration contract

Wena uses one SQLite database for local and server mode. Schema version 2 adds the
scoped `card_descriptions` table; version 3 adds scoped checklists and items, and
version 4 adds the measured card-scoped ordered item index. The historical golden in
`server/migrations/001_initial.sql` retains its exact published bytes and checksum;
versions 2 through 4 are additive migrations. The artifact footer format is
unchanged, and its payload is the exact concatenation of all four SQL files. The application
opens SQLite with
foreign keys enabled, a bounded busy timeout, WAL journalling and full synchronous
writes. Startup accepts only the exact current version or applies each missing
forward-only migration in order inside `BEGIN IMMEDIATE`; a migration checksum or
version mismatch fails closed. Downgrades and partially applied migrations are not
supported. The runner owns a compiled ordered migration registry and executes
only its exact reviewed SQL. It checks version/history while holding the writer
lock, and verifies all history rows rather than only the current row. Missing,
extra, incorrectly typed or wrong-checksum rows fail closed. Open, backup and
restore share this validator, which preserves an existing caller transaction.
It validates migration metadata and the exact compiled definitions of the new
v2-v4 tables and indexes, including their FK, NOT NULL, size and NUL constraints. A complete
historical v1 domain-schema fingerprint remains future work. Version-0 databases containing unrelated user objects are refused.

Boards own swimlanes and lists. Cards reference their board, swimlane and list; all
parent deletions are restricted. Stable integer `position` columns and unique parent
position indexes provide deterministic ordering. Every mutable row has an optimistic
`version`. Sessions reference actors. Idempotency rows use the complete actor, route,
operation and request-version tuple, and are committed in the same transaction as the
domain mutation and response checksum.

Before serving, startup runs `PRAGMA quick_check`; maintenance and restore run the
full `integrity_check` and `foreign_key_check`. Any failure keeps the listener closed.
Unexpected process termination relies on WAL recovery; tests must kill between begin,
WAL write and commit and prove either the old or complete new transaction, never a
partial hierarchy or idempotency record.

Backups use SQLite's online backup API into a new sibling temporary file, checkpoint
that copy, run integrity and foreign-key checks, compute a SHA-256 checksum and fsync
the file and parent directory before an atomic rename. Restore never overwrites the
only database: verify checksum and schema in a separate file, close the listener,
rename the current database to a recoverable generation, atomically install the
verified copy, fsync, reopen and recheck. Corrupt/truncated/WAL-only/wrong-checksum or
newer-schema inputs are rejected while the previous generation remains recoverable.

The SQLite transaction adapter, online backup, guarded restore and compiled
migration runner are implemented and covered by native regression tests. The
backup/restore fsync and publication guarantees described above remain contract
goals where the current implementation does not yet perform those steps; schema
validation is not evidence of completed power-loss durability. The in-memory
adapter is test-only and is not a fallback for a corrupt production database.

Historical v1/v2/v3 payloads still select their original schema and refuse
downgrades. Current artifacts carry the verified v1-v4 bundle: blank creation
and older-schema upgrades are atomic, and repeated opens do not rewrite migration
history. An older backup can be restored under a newer compiled target: the copied bytes are rechecked, the
private copy is upgraded with compiled SQL and DELETE journaling, and integrity
is checked before the listener stops. Failure keeps the source backup and live
database unchanged. Current backups retain descriptions, checklists and items.

The 1024-byte description limit is versioned; future increases require migration
and command/input-capacity review. Empty/multiline text is stored literally;
UTF-8 and control-character validation belongs to the mutation adapter. The
implemented description editor and checklist mutation/UI contracts are described
in [native-desktop.md](native-desktop.md) and [checklists-design.md](checklists-design.md).

## Connection hardening

Application-owned SQLite connections are configured immediately after opening
with `SQLITE_DBCONFIG_DEFENSIVE=1` and `SQLITE_DBCONFIG_TRUSTED_SCHEMA=0` through
`wena_sqlite_connection_harden()`. This includes writable database opening,
backup destinations, readonly restore validation and staged restore upgrades. Desktop readonly
preflight uses the same helper. The helper checks both SQLite return codes and
resulting flag values; callers close the connection if either protection cannot
be applied. Builds with headers lacking either configuration flag retain C89
compilability but cannot silently open an unprotected application connection.

The policy follows SQLite's [security recommendations](https://sqlite.org/security.html)
and [connection configuration reference](https://sqlite.org/c3ref/c_dbconfig_defensive.html).
It blocks SQL-level catalog corruption through `writable_schema` and prevents
untrusted schema objects from invoking unapproved application-defined functions.
It preserves ordinary SQL triggers, including injected rollback tests. It does
not authorize a user, replace SQLite engine updates, or certify arbitrary
database files as safe. Compiled migration identity checks remain in force.

Snapshot loading and validation of caller-owned backup sources do not silently
change those connections' configuration. A caller that opens its own database
connection controls its initialization policy and may invoke the helper
explicitly. Tests cover both that ownership boundary and active protection on
Wena-owned writable/readonly connections, plus normal migration, mutation,
backup, and restore behavior.
The implemented checklist schema and its validation evidence are documented in
[schema-v3-storage.md](schema-v3-storage.md).

Schema-v4 `004_checklist_item_card_order.sql` adds
the measured selected-card ordered index without changing v1-v3. See
[checklist-query-work.md](checklist-query-work.md) for exact query and upgrade evidence.

Schema-v5 `005_labels.sql` adds board-owned labels
and scoped card assignments through the same immutable migration chain. See
[schema-v5-storage.md](schema-v5-storage.md) for field contracts, exact color
semantics, indexed query work, upgrade and restore evidence.

The current artifact target is schema-v6. `006_board_settings.sql` stores the
opt-in compact checklist count setting in a board-owned extension table, with
missing rows defaulting to false. See [schema-v6-storage.md](schema-v6-storage.md)
for the additive-table architecture decision and immutable upgrade/restore tests.
