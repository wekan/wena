# Exact cross-column card insertion

The shared `WENA_DOMAIN_MOVE_CARD` SQLite operation accepts `insertPosition` for
an exact ordinal in a different list or swimlane on the same board. This is the
storage foundation for native form and drag insertion; those controls are not
connected yet. Existing append moves and same-column `targetPosition` reorders
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
