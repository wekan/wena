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
List revision checks will use the existing list version, not an independent
setting version. The board/archive-state/list index supports the future scoped
archive browser. This migration does not itself expose archive actions.

All v1-v9 migration bytes remain immutable. The shared migration harness verifies
upgrades from each prefix, preserved card flags and list versions, defaults,
strict scalar checks, cross-board foreign-key rejection, table/index/metadata/
commit failure rollback, downgrade rejection and reopening. Embedded artifacts
must contain the newly pinned complete migration bundle. Native snapshot loading,
guarded archive/restore mutations and menu/archive-browser integration remain
open in ROADMAP.md.
