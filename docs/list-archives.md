# Native list archive persistence

WeKan's `models/lists.js` archives an ordinary list by setting its archive state
and `archivedAt`, and restores it by clearing the state. It archives/restores
children separately only for template lists. Wena's ordinary-list implementation
must therefore retain each card's own archive flag and position. Template list
and swimlane cascades remain separate work until their model semantics are ported.

Schema v10 adds `list_archive_state`, keyed by the exact list and board. An absent
row means an ordinary list has not been archived; new rows default to unarchived
with timestamp zero. The timestamp is nonnegative integer epoch milliseconds and
can be retained when restoring, matching WeKan's retained `archivedAt` field.
List revision checks use the existing list version, not an independent
setting version. The board/archive-state/list index supports the future scoped
archive browser. This migration does not itself expose archive actions.

All v1-v9 migration bytes remain immutable. The shared migration harness verifies
upgrades from each prefix, preserved card flags and list versions, defaults,
strict scalar checks, cross-board foreign-key rejection, table/index/metadata/
commit failure rollback, downgrade rejection and reopening. Embedded artifacts
must contain the newly pinned complete migration bundle. Native snapshot loading
and guarded archive/restore mutations are now
implemented. Native adapters and menu/archive-browser integration remain open
in ROADMAP.md.

The typed local archive and restore operations use the same actor, board,
optimistic version, durable request identity and transaction boundary as other
mutations. No-op validates the current version without advancing it or reserving
a request. Changed state and the list revision are reread before commit, so
ignored or altered writes roll back. These operations do not introduce HTTP
routes. Atomic board snapshots load archive state with the hierarchy in their
existing read transaction, retain the complete card cache and reject malformed
or wrong-board metadata. Legacy databases omit the table; v10-or-newer databases
with a missing archive table fail instead of silently unhiding lists.

Native hierarchy movement includes hidden siblings in its complete order and
fingerprint. Its numeric selector uses those same ordinals, and rendered drag
handles retain the original snapshot indices across hidden entries. Archived
model sources cannot open a move panel; archiving the source during a gesture
cancels it. Moving an active sibling preserves the hidden list's archive state,
timestamp and revision. Database archive-state eligibility checks for other
mutations remain required before exposing native archive controls.
