# Atomic native checklist entry

The existing Add an item to checklist editor has an optional canonical WeKan
`newlineBecomesNewChecklistItem` checkbox. In multiline mode, Enter inserts a
newline and only Save submits. Cancel and focused Escape abandon the draft.
The existing single-item path retains its title input and keyboard behavior.
Switching back to single-line mode refuses to discard a multiline or oversized
draft; the user can edit it or cancel.

The native batch limit is **eight items, 128 UTF-8 bytes per trimmed title, and
1,031 raw input bytes**. The raw bound includes whitespace and blank lines before
parsing. An empty result, ninth item, invalid scalar UTF-8, NUL, control characters,
oversized title or oversized raw input rejects the entire batch. LF, CR and TAB
are allowed in raw input for canonical splitting/trimming; internal CR or TAB in
a final title is rejected by the existing title model. Unicode C1 controls are
rejected. Duplicate titles remain separate items. Only LF splits lines; canonical
ECMAScript trim and blank removal reuse the shared model parser. Append preserves
the original order, so the native action does not need a reverse checkbox.

The editor reserves four overflow bytes, enough to retain a complete four-byte
UTF-8 scalar beyond the raw limit for validation. A real Nuklear regression
verifies this boundary. This does not claim complete operating-system clipboard
or IME behavior; the reviewed Nuklear clipboard and platform input paths remain
separate from this bounded local mutation contract.

`WENA_CHECKLIST_ADD_ITEMS` and native domain operation `add-checklist-items` reuse
the existing typed checklist callback and outer guarded SQLite transaction.
The new backend lives in `server/mutations/checklist_batch.[ch]`; no schema,
global transport capacity, dependency or alternate transaction architecture is
introduced. Eight all-percent-encoded maximum titles require 3,093 bytes before
the small scope/version envelope and fit the unchanged 4,097-byte domain body.

The backend checks the selected active card, exact board/card/checklist scope,
expected card and checklist versions, complete-card item capacity of 1,024,
integer positions and append overflow before the first insert. It includes
scope-corrupt rows when checking ownership, even on a caller-owned connection
with foreign keys disabled. IDs are deterministic SHA-256 values derived from
the existing actor/route/request/operation identity and item ordinal. Each new
item starts unfinished at version 1. The card and checklist versions advance
exactly once per accepted batch, and the checklist timestamp never goes
backwards. Version bounds retain a readable resulting version on 32-bit and
64-bit `long` platforms.

The existing outer transaction checks actor existence and replay, then writes
one idempotency record and response checksum for the whole batch. Any child,
parent, card, metadata or commit failure rolls back all inserted rows and version
changes. Reusing a committed request is rejected without inserting duplicates,
including after database reopen. A successful write followed by a failed UI
reload disables resubmission until Refresh succeeds.

Validation: `test_checklist_batch.sh` covers single/eight-item writes, maximum
encoded reserved characters, whitespace/duplicates/order, invalid input,
scope/actor/archive guards, stale versions, replay, row and metadata rollback,
position and collection capacity, stable IDs and database reopen.
`test_nuklear_checklist_batch.sh` exercises the actual checkbox, typing and
newline behavior, Save/Cancel/Escape, failed mode switch, ninth-item and four-byte
UTF-8 overflow, retained failed draft and failed post-commit reload. Existing
fake and real Nuklear checklist suites also continue to pass.
