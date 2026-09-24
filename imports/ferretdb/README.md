# Read-only SJSON snapshots

`sjson` validates canonical FerretDB v1 SJSON against the pinned layout in
[the compatibility contract](../../docs/ferretdb-sqlite-compatibility.md).
This original MIT-licensed C89 implementation shares the bounded JSON reader;
it adds no rendering, database or third-party codec dependency.

Initialize a `WenaSjsonDocument *` to NULL and pass its address to
`wena_sjson_parse`. Success replaces the old snapshot; failure leaves it intact.
Release with `wena_sjson_free`. Root value 0 is an object. Follow `first`/`next`
until `WENA_JSON_NONE`, or use `wena_sjson_member` with an explicit key length.
Object children follow `$k`, independent of the JSON object's textual order.
Each value retains JSON node indexes for its value, descriptor and optional name.
Exact integer/date/timestamp text and binary/regex metadata remain accessible
through the owned JSON document; no conversion through a double is required.

All schema descriptors are checked, including descriptors for null values.
The pinned decoder treats JSON null as BSON null regardless of its known type;
that behavior is retained. Unknown types, extraneous descriptor metadata,
missing/extra fields, duplicate field order entries, wrong scalar types,
out-of-range integers and double overflow fail before publication. Canonical
metadata is required: this is deliberately narrower than accepting unused known
metadata fields. Binary validation accepts standard padded base64 and CR/LF.
Double validation uses the current C numeric locale without changing it; as with
other C locale APIs, callers must not change the process locale concurrently.
JSON reader byte/node/depth limits also apply.

This is a typed read-only view, not a BSON encoder or permission to modify an
existing FerretDB database. Codec writes and real copied-database parity remain
separate roadmap work. Run the fast `sjson` suite through `scripts/wena.py`.
