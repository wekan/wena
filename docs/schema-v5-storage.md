# Schema-v5 label storage

Migration `005_labels.sql` extends the existing compiled migration registry with
board labels and card assignments. Published v1–v4 SQL remains byte-for-byte
unchanged; the verifier now pins v4 explicitly alongside v1–v3. The current
artifact identifies the concatenated v1–v5 bundle. Historical prefixes continue
to select their original target and reject downgrades from a newer database.

This document records the schema and migration slice. Native label models,
mutations and presentation are integrated separately and require their own
combined validation. No new runtime dependency, migration framework, cached
assignment count or unrelated WeKan collection is introduced.

The mapping follows pinned WeKan `689a393841f08c3a020a4ef435b869b7641b21df`:
`models/boards.js` embeds board `labels` with `_id`, optional `name` and `color`;
`models/cards.js` stores a set of `labelIds`; `models/metadata/colors.js` imports
its 25 named colors from `config/const.js`. Native relational rows preserve that
board ownership rather than imposing globally unique label IDs. This is not a
claim that the relational schema can be used directly as a Meteor collection.

| Stored field | Contract |
| --- | --- |
| `labels.board_id, id` | Composite TEXT NOT NULL primary key; each is 1–64 bytes without NUL |
| `labels.name` | TEXT, 0–128 bytes without NUL; default empty string |
| `labels.color` | TEXT; empty/default, one of 25 exact palette names, or exactly `#` plus six hexadecimal digits |
| `labels.position` | INTEGER 0–2147483647, unique within board; deletion may leave gaps |
| `labels.version` | Positive INTEGER, default 1 |
| `labels.created_at` | Nonnegative INTEGER UTC epoch milliseconds, default 0 |
| `labels.updated_at` | INTEGER UTC epoch milliseconds, at least `created_at`, default 0 |
| `(board_id, name, color)` | Unique exact BINARY pair, matching canonical duplicate prevention |
| `card_labels` | Composite primary key `(board_id, card_id, label_id)`; no duplicate assignment |
| Assignment ownership | Composite foreign keys to `cards(board_id,id)` and `labels(board_id,id)` |

Empty names are valid. Empty color is stored unchanged; rendering resolves the
native default through the shared palette. Custom hexadecimal color case is
preserved, so differently cased strings remain distinct exact pairs as in the
canonical source. Names are never trimmed. SQL prevents embedded NUL, including
inside custom colors; adapters additionally enforce native identifier, UTF-8
and control-character rules. SQLite affinity may convert an input value before
these stored-type checks run. The 128-byte and ordinal limits are explicit
native bounds, not unrestricted WeKan round-trip claims.

All foreign keys use `ON DELETE RESTRICT`. A future or integrated deletion
adapter must remove assignments and the label explicitly in one guarded
transaction. Archiving a card retains its label assignments. Label metadata
changes use the board version; real assignment changes also increment that
board version and the affected card version. This makes a stale label deletion
notice a concurrent assignment using the existing aggregate guard, at the cost
of serialization between otherwise independent label changes on the same board.
The schema does not hide these changes in triggers or duplicate versions on
assignment rows.

The label position uniqueness index serves complete-board ordered loading.
The reverse index `card_labels_label_cards_idx(board_id,label_id,card_id)`
serves scoped label deletion and the loader's derived assignment counts. The
selected-card loader deliberately reads assignments without a board predicate,
so a corrupt foreign-board assignment cannot be silently filtered out. Its
`card_labels_card_order_idx(card_id,label_id,board_id)` index avoids scanning
unrelated cards while preserving that validation behavior.

The v5 query-work test pins its projections to the integrated loader when
present and seeds 10,000 unrelated board labels and assignments. Selected board,
selected card and selected label queries take 134/138, 15/15 and 19/20 SQLite VM
steps before/after, with zero full-scan or sorting steps on tested SQLite 3.51.3.
Removing the card-first index inside a rollback-only fixture causes at least
10,000 scan steps and over 30,000 VM steps for the same selected-card query.
Complete-board loading and its derived counts still grow with labels and
assignments on that board; the native adapter must enforce its collection
capacity. These indexes do not impose an artificial SQL LIMIT that would hide
malformed data.

`tests/test_sqlite_schema_v5.sh` covers fresh creation and populated v1/v2/v3/v4
upgrades, preserving descriptions, checklists, item state, versions and existing
idempotency records. It checks defaults, every named color, custom color case,
maximum native name/position values, 64-bit timestamps, 63 rejected type/scope/
uniqueness/color mutations, archive retention, repeated opens, historical target
refusal and the indexed queries above.

Twenty-one damaged history/table/index fixtures are refused by schema validation,
backup, restore and open. Late DDL, history-write and commit refusal roll back to
a complete v4 database. Process termination at those stages reopens complete
v4; termination after commit retains complete v5. Four simultaneous v4 upgraders
produce exactly five history rows. Old backups upgrade inside the verified
private staging copy before the listener stops; staging failure leaves the live
generation unchanged, listener restart failure restores it, and source backup
bytes are preserved. A current v5 backup also restores with all label rows and
assignments intact.

The new suite passes strict C89 and ASan/UBSan against SQLite 3.51.3 with
LeakSanitizer disabled for this container. All 102 existing suites also pass in
the isolated schema worktree, zero failed or skipped; the new v5 suite is run
separately before main catalog registration. Main-worktree label UI/backend
integration results belong in the current work-session report. Physical v1 schema fingerprinting, whole-file
publication durability across power loss and complete remote WeKan parity remain
outside this slice.
