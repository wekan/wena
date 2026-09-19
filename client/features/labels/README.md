# Board labels and card assignments

`store.[ch]` defines the complete bounded snapshot and guarded edit intent.
`mutation.[ch]` adapts those contracts to the existing SQLite transaction and
idempotency path. `panel.[ch]` owns a detached snapshot and bounded Nuklear draft;
`component.[ch]` draws a reusable canonical-color badge with readable black or
white text. The desktop chooses board mode or an exact active-card scope.

The native slice supports up to 128 labels per board, creating and editing a
name/color, assigning or unassigning a selected card, and explicitly confirming
label deletion. The confirmation shows the saved label and the affected-card
count, including archived cards. It does not claim an activity-history feature.
Cancel and Escape do not write. A failed save retains an editing draft; a
successful save followed by a failed reload disables writes until Refresh.

Names may be empty and contain up to 128 UTF-8 bytes, with no embedded NUL or
C0/DEL/C1 controls. Valid drafts use the shared ECMAScript whitespace trim helper
before submission. Backend callers retain exact supplied bytes. The palette
uses the 25 canonical named colors, initially choosing the first unused color;
custom input requires a complete `#RRGGBB`. An existing empty color remains an
empty stored value and renders white. Equal names or equal colors are allowed;
an identical name/color pair is not duplicated. Stable IDs identify all edits.

The panel captures board, label and optional card revisions before editing.
Deleting a label removes its assignments atomically and updates every affected
card, including archived cards. No-op and replay semantics are documented in
`mutation.h`; the panel always reloads after an accepted intent. Linked cards,
native drag/drop label ordering, activity history and full Meteor parity remain
outside this slice; see `docs/labels-source-contract.md` for pinned source facts.

Tests: `test_labels.sh` checks state and callback behavior;
`test_nuklear_labels.sh` checks real input, keyboard handling, destructive
confirmation, and drawn background/text colors; `test_labels_sqlite.sh` checks
the complete UI-to-SQLite path, rollback, versions, scopes, replay and reopening.
