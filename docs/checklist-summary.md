# Compact minicard checklist counts

`client/features/checklists/summary.[ch]` is a read-only board projection for
compact `finished/total` badges. It does not render expandable checklist contents
or introduce another checklist store. The desktop must load it when the badge
is enabled and refresh it after committed mutations; rendering and binary
snapshot lookup perform no SQL.

Pinned WeKan `689a393841f08c3a020a4ef435b869b7641b21df` defines
`boards.allowsChecklistCountBadgeOnMinicard` with default `false`. The helper in
`client/components/cards/minicard.js` checks that boolean before the Jade
component requests any counts. An enabled card with one empty checklist has a
`0/0` badge; a card with no checklists has no badge. The finished appearance
requires at least one item and all actual items finished.

The card model sums all checklist items and their `isFinished` values.
`hideCheckedChecklistItems`, `hideAllChecklistItems` and per-checklist
`showOnMinicard` do not filter this compact count. In particular, a hidden
unfinished checklist does not make the card's count finished. Its separate
checklist-level `isFinished()` helper has different hide-all behavior; the
projection does not confuse those contracts.

The explicit `enabled` parameter defaults to zero in a newly allocated snapshot.
A disabled load publishes an empty disabled snapshot without SQL or allocation.
A real load validates actor/board scope and owns one consistent read transaction.
It publishes only after commit, preserving the caller's previous bytes on
failure. It refuses caller-owned transactions. Board and card versions are
included for cache invalidation; this local actor check is not remote login or
membership authorization.

The heap snapshot stores only IDs, versions, flags and aggregate progress for at
most 2,048 actual board cards, including archived cards. Checklists and items are
validated one at a time using the existing model rules. Each card permits at
most 64 checklists and 1,024 total items. No item-title collection is retained.
The fixed five read queries plus BEGIN/COMMIT use existing board-card, checklist
parent and schema-v4 item-order indexes; there is no query per card.

Child rows are selected by actual board card IDs before checking their claimed
board, preventing a bad foreign-board row from disappearing behind a filter.
The reverse parent query also rejects an item attached to a selected checklist
but claiming another or nonexistent card. Orphan rows whose claimed board is the
only connection to this board, with neither an actual selected card nor selected
checklist parent, are outside this projection's scope. Checking those rows here
would require a global scan or a separate board-first item index. Database-wide
integrity checks remain responsible for that broader boundary.

`tests/test_checklist_summary.sh` covers disabled/empty/hidden/completed behavior,
archived counts, invalid scope and typed values, native capacity boundaries,
byte-for-byte failure atomicity, failed commit and persistence on reopening.
A second WAL connection commits an item change after the read snapshot begins;
all counts and versions remain from the original generation until the next
load. All 2,048 cards still use seven SQL statements. Adding 10,000 unrelated
checklists/items changes measured work from 398 to 400 SQLite VM steps, with zero
full-scan steps; removing the existing v4 index produces over 10,000 scan steps.
Strict C89 and ASan/UBSan pass on SQLite 3.51.3, with LeakSanitizer disabled for
the container. Board setting persistence and real desktop badge controls require
their separate integrated tests.
