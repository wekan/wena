# Bounded selected-card checklist queries

Schema-v4 migration `004_checklist_item_card_order.sql` adds only
`checklist_items_card_order_idx(card_id,checklist_id,position,id)`. All v1-v3
migration bytes remain immutable. The query and its returned rows are unchanged;
native loading still reads every selected-card row and rejects malformed scope.

The actual native query selects checklist-item fields with `WHERE card_id=?2`
and `ORDER BY checklist_id,position,id`. Schema 3 indexed only checklist and
position, forcing a global index scan to find one card's items. The new index
supports both the card lookup and the complete existing order. A three-column
candidate requires temporary sorting for the final ID term on the pinned engine.

Measurements below use a compiled C executable linked to the project's SQLite
**3.51.3**, source ID
`737ae4a34738ffa0c3ff7f9bb18df914dd1cad163f28fd6b6e114a344fe6d618`.
These are `sqlite3_stmt_status` instruction/full-scan/sort counters, not timings.
The fixture contains eight selected items and 12,000 unrelated items across
100 other boards; doubling adds another 100 boards and 12,000 unrelated items.

| Query index | VM steps | Full-scan steps | Sort operations |
| --- | ---: | ---: | ---: |
| Schema 3, checklist/position | 48,121 | 12,007 | 0 |
| Candidate card/checklist/position | 305 | 0 | 8 |
| Schema 4, card/checklist/position/ID | 114 | 0 | 0 |
| Schema 4 after doubling unrelated items | 114 | 0 | 0 |

The regression verifies an indexed SEARCH plan, zero full-scan/sort counters and
a generous bounded instruction budget instead of exact timing thresholds. It
also checks the existing checklist, card, list, swimlane and card-version scope
queries: each uses an indexed search, visits no unrelated rows through a full
scan and executes fewer than 256 VM steps for this selected fixture. The native
loader's query literal is checked against the performance fixture when present
in the integration workspace.

Old v1/v2/v3 bundles retain their original targets. Tests cover upgrades to v4,
reopen/downgrade refusal, current backups, older backups upgraded in private
staging, failed staging without listener interruption, failed restart rollback,
unchanged source backups, failed index/history/commit stages and altered-index
rejection. No runtime migration framework or other application field is changed.

Local validation on 2026-09-07: **71 isolated native suites passed**, zero failures
or skips; the new v4 suite also passed **ASan/UBSan**, with LeakSanitizer disabled.
The C measurement and final regression use the pinned project library; the
Python fixture-preparation module's separate SQLite version is not measurement
evidence. Full integrated-main testing follows the controlled merge.
