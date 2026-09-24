# Exact cross-column card insertion

The shared `WENA_DOMAIN_MOVE_CARD` SQLite operation accepts `insertPosition` for
an exact ordinal in a different list or swimlane on the same board. This is the
storage foundation for native form and drag insertion. The native mutation
adapter and drag insertion zones are implemented; the explicit Move form
still needs its cross-column position control. Existing append moves and same-column `targetPosition` reorders
retain their contracts.

The request includes the existing `cardId`, `expectedVersion`, `targetListId`
and `targetSwimlaneId`, plus `sourceListId`, `sourceSwimlaneId`,
`expectedSourceOrder`, `expectedOrder` and `insertPosition`. Both fingerprints use
the shared ordered-ID/exact-position serialization. The destination ordinal is
zero-based and includes archived cards; its final boundary means append. Empty
destinations use ordinal zero and the empty SHA-256 fingerprint. Combining
`insertPosition` and `targetPosition`, a stale fingerprint, an archived source,
a wrong revision, unknown actor/scope, repeated request or an out-of-range ordinal
fails before commit.

Insertion and reordering share one bounded column reader and two-pass position
writer. A column has at most 2,048 cards; insertion requires room for one more.
The destination becomes contiguous while source sibling positions stay intact.
Only the moved card advances its revision, exactly once. A post-write read checks
the destination order, revisions and archive flags, plus unchanged source
siblings, before the existing request
metadata/commit boundary. Early and late errors roll back all changes.

The WeKan reference is `models/cards.js` (`move`) and
`client/components/cards/cardDetails.js` (`moveCardPopup`/`relativeCardSort`):
a selected target supplies a relative position and an absent target appends.
Wena's relational layer currently uses bounded integer ordinals instead of
Meteor's fractional sort values. Cross-board remapping, move activity/provider
semantics and full native interaction parity remain separate roadmap work.

`card-insert` tests both lists and swimlanes, every insertion boundary, empty
columns, archived siblings, duplicate titles, capacity, malformed/stale requests,
replay, staging/metadata rollback and unexpected trigger changes. The suite is
strict C89 and also runs with the existing ASan/UBSan compiler wrapper.

The native `wena_card_mutation_insert[_request]` adapter captures both columns
through `models/card_order`, which also represents an empty column without an
allocation. It prepares a complete replacement cache before SQLite begins its
mutation: unaffected cards preserve their traversal order and destination cards
are published together in their new ordinal order. After commit only a memcpy
and frees remain. Failure preserves the original cache byte-for-byte. The same
fingerprint helper now serves native same-column reordering and insertion.

During a drag, existing card handles offer a translated destination zone before
each visible card in another column. Its displayed one-based ordinal includes
archived and filtered siblings. Release captures an immutable destination order;
the post-render apply checks it again before calling the insertion adapter. The
existing column destination remains the append/empty-column target. Disabled or
clipped handles cannot accept insertion, and cancellation frees both snapshots.
