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

## Codec foundation

Pinned `internal/handler/sjson/sjson.go`, `schema.go`, `document.go`, `array.go`
and the scalar modules establish that `$s.p` contains field descriptors and `$s.$k`
preserves field order. Arrays carry one descriptor per element. Signed 64-bit
values and unsigned 64-bit timestamps are JSON numbers, so converting all numbers
to floating point would lose information. Dates are signed epoch milliseconds;
ObjectIDs are 24 hexadecimal characters, binary values use base64 plus a subtype,
and doubles additionally support the schema-disambiguated string `NaN`.

`imports/json/document` now supplies a shared bounded syntax reader that preserves
those exact number lexemes and object order, decodes scalar UTF-8/JSON escapes and
rejects duplicate decoded names. Fast C89 and ASan/UBSan tests pass. This is a
syntax foundation. `imports/ferretdb/sjson` builds an owned typed snapshot over it,
validating all 13 BSON types, exact integer ranges, base64/subtypes, ObjectIDs,
regex metadata, nested array descriptors and ordered object fields before publication.
It accepts canonical descriptors only (irrelevant metadata fields are rejected),
retains the pinned decoder's JSON-null override and rejects double overflow.
Neither reader changes the database. Typed writes, concurrent-owner locking and
copied-WeKan round trips remain required before enabling direct replacement.
