# Shared bounded JSON reader

`document` provides one C89 reader for SJSON/import and future REST adapters.
It follows [RFC 8259](https://www.rfc-editor.org/rfc/rfc8259), with explicit
interoperability restrictions: duplicate decoded object names, lone UTF-16
surrogates, invalid UTF-8 and a byte-order mark are rejected. Object order and the
exact spelling of JSON numbers are retained. Type-specific numeric range checks
belong to the caller; this parser never rounds a 64-bit integer through `double`.

The snapshot owns the original input and decoded strings. Embedded escaped NULs
are represented with explicit string lengths, including in object names. Queries
use decoded names rather than interpolated JSON paths. Limits are 1 MiB of input,
4,096 nodes and 32 levels below the root; exceeding a limit leaves an existing
snapshot unchanged. Arrays store ordered children; object children alternate key
and value nodes. Callers must not modify the published nodes or linked indexes.

The reader reuses `models/text` for scalar UTF-8 decoding and has no database,
filesystem, network or locale dependency. This is original project code under
Wena's MIT license; no additional rendering/parser dependency is introduced.
`test_json_document.sh` covers precision, ordering, escapes, malformed/duplicate
input, atomic replacement, all limits and every byte substitution of a nested
sample. Run it with the existing sanitizer compiler wrapper for memory checks.
