# FerretDB v1 SQLite compatibility contract

Discovery is pinned to FerretDB commit `9ab2ca69d43ac17bb03a4c92e8ab5e72bfadb778`.
Its SQLite URI names a directory and maps MongoDB database `wekan` to
`<directory>/wekan.sqlite`. Collections are rows in `_ferretdb_collections`; each row
maps the Mongo collection name to an opaque, UUID-derived physical STRICT table.
Documents are not relational rows: each is FerretDB SJSON in the single
`_ferretdb_sjson TEXT NOT NULL` column, with BSON type information below `$s`.

The observed WeKan database at `state-debug-speed/wekan.sqlite` uses WAL, SQLite
`user_version=0`, `application_id=0`, and collection settings `indexFormat:2`. Therefore
SQLite header integers alone are not a format version. Wena recognizes this exact
metadata/SJSON/index-format fingerprint and otherwise fails closed.

The first implementation is read-only. It opens an absolute path with
`SQLITE_OPEN_READONLY`, enables `query_only`, checks quick integrity, metadata JSON,
physical SJSON tables, and required boards/lists/swimlanes/cards mappings. It never
creates Wena relational tables in a FerretDB file.

Before any future write support, Wena must acquire an exclusive application lock and
SQLite immediate write lock, refuse a concurrent FerretDB/Wena owner, checkpoint WAL,
make and verify an online backup plus SHA-256 sidecar, then update SJSON and metadata
using FerretDB's exact codec/index rules in one transaction. Unknown format, corrupt
JSON/SJSON, missing metadata/table/index, unsupported index format, failed backup,
busy lock, or integrity failure must leave the database byte-for-byte unchanged.
Migration is forward-only with an atomic rollback and recovery marker; restore uses
the already verified stop/swap/reopen/rollback lifecycle. Direct replacement remains
blocked until SJSON codec parity and a real copied-WeKan write/round-trip suite pass.
