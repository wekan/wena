# Native entity version boundaries

`models/version.h` defines one shared range for board, hierarchy, card, checklist,
item, and label versions:

| Role | Inclusive range |
| --- | --- |
| Readable persisted entity version | `1 .. WENA_VERSION_READ_MAX` (`LONG_MAX - 1`) |
| Accepted expected version in a mutation | `1 .. WENA_VERSION_MUTATE_MAX` (`LONG_MAX - 2`) |

Every accepted version increment therefore produces another readable integer.
The last readable version remains available to snapshot and editor loaders, but
further mutations reject. This applies to same-value saves and same-position
requests as well: an exhausted entity cannot be used to start a mutation, even
when that particular request could otherwise be a no-op. A rejected request
creates no idempotency row and leaves the existing state intact. No version is
silently reset, wrapped, saturated, or converted to a floating-point value.

SQLite's integer range can exceed the platform's `long` range. The explicit
portable C89 bounds retain the existing native reader policy while reserving room
for the increment. Entity versions are independent of request identities,
timestamps, and ordering positions; those contracts retain their separate bounds.
No SQL migration or immutable migration bytes are changed by this correction.

Version loaders require actual SQLite INTEGER values, reject zero and values
beyond the readable range, and publish validated output only after a successful
read. Board snapshots now enforce the same upper bound as editor snapshots. The
card title loader also rejects REAL values instead of accepting their truncated
integer conversion.

Card and hierarchy reorder preserve sibling versions. A sibling at the readable
terminal value remains valid and does not prevent a different selected entity
from moving. Selected rows must still have an incrementable version. Label
deletion additionally validates the versions of every affected card, including
archived cards, because all those rows receive a version increment together.

`tests/test_version_boundaries.sh` checks the final legal increment and exhausted
rejection across title, description, archive/restore, hierarchy/card movement,
checklist creation/edit/display/deletion, and batch item insertion. It also checks
native adapter loaders and terminal no-ops, unchanged sibling versions during
indexed card and hierarchy movement, rejected noninteger values, metadata-failure
rollback after child writes, replay, and terminal data after reopening. The
existing form-validation regression separately confirms that request identities
keep their prior bound while entity responses stop at the readable terminal.

These tests use the existing migration files as fixtures; implementation schema
ownership, application upgrades, and restore validation remain unchanged.
