# Card section preferences

The generic `models/card_section` model and `imports/preferences/sections` SQLite
adapter store an actor's collapse choice for a card section. This follows WeKan's
`models/users.js` `profile.collapsedCardSections[cardId][sectionKey]`, including
`checklist-<id>` keys shared by minicards and the opened checklist panel. Other
section keys can use the same store and `client/components/common/card_section`
control without copying persistence or rendering logic.

Schema v8 adds `actor_card_sections`; earlier migration bytes remain immutable.
The key is actor/card/section. Board scope is derived from the card, rather than
copied into a preference that could disagree with its owner. Missing preferences
mean expanded. Explicit choices have optimistic revisions; a first write expects
zero, later writes expect the captured revision. A guarded no-op makes no change.
A preference write does not advance a card or board revision or create a canonical
board activity. The actor still comes from the trusted local desktop session;
this is not a login or membership authorization implementation.

Keys are 1–128 ASCII letters, digits, underscores or hyphens. Snapshots allocate
only for existing rows and permit up to 128 section preferences per actor/card,
with the board's existing 2048-card aggregate bound. Reads use one transaction and
publish an owned sorted snapshot only after commit; binary-search lookups perform
no SQL. Writes require an active card in the selected board, an existing actor,
and a matching preference version. Late failures roll back; a completed intent is
never automatically retried. Failed reads leave the old snapshot allocated but
disable controls until an explicit refresh succeeds.

The desktop refreshes after local writes and when opening the checklist panel.
Other processes' changes are checked by revisions at save and picked up on
refresh; there is no cross-process notification claim. Smoke mode only reads
preferences. The older list/swimlane sidecar format is unchanged.

Tests use actual SQLite and Nuklear to cover actor isolation, default/no-op/stale
writes, malformed values, terminal revisions, capacity, rollback, reopen, v7-to-v8
upgrade rollback and shared state between the two UI locations. Drawing and
lookups issue no SQL.
