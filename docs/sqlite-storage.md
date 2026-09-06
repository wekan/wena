# SQLite storage and migration contract

Wena uses one SQLite database for local and server mode. Schema version 1 is the
golden in `server/migrations/001_initial.sql`. The application opens SQLite with
foreign keys enabled, a bounded busy timeout, WAL journalling and full synchronous
writes. Startup accepts only the exact current version or applies each missing
forward-only migration in order inside `BEGIN IMMEDIATE`; a migration checksum or
version mismatch fails closed. Downgrades and partially applied migrations are not
supported.

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

SQLite adapter implementation remains pending until the migration runner enforces
this contract. The in-memory persistence adapter is test-only and is not a fallback
for a corrupt production database.
