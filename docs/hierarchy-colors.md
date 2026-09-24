# Native hierarchy colors

WeKan's `models/lists.js` normalizes list/swimlane colors to its canonical item
palette or an exact six-digit hex value. Missing/empty color uses the default;
custom colors receive readable foreground contrast. Wena already shares the
25-color palette, strict hex validation and contrast calculation with labels.
The reusable native color input keeps palette/custom drafts independent from
persistence and never submits the enclosing form on Enter in the hex field.

Schema v11 adds `list_colors` and `swimlane_colors`. Separate tables enforce exact
board/parent foreign keys for each hierarchy type, with one color per parent and
board-scoped indexes. Both use the same constraints as existing label colors:
empty default, canonical names, or `#RRGGBB`, retaining hex case. Invalid names,
short/long/non-hex values, BLOB/NULL/NUL values and cross-board parents fail.
Existing hierarchy revisions will guard color mutations; there is no independent
color revision. No existing migration or hierarchy/card row is rewritten.

The common migration harness covers upgrades from every v1-v10 prefix, preserved
board/list/swimlane/card state and archive metadata, defaults, every palette name,
custom hex, parent restrictions, reopening and downgrade rejection. Injected
failures at both tables, both indexes, migration metadata and commit roll back
all v11 objects and permit retry. The compiled registry pins v1-v10 bytes and
verifies exact v11 DDL in each embedded migration bundle.

Typed local list/swimlane color mutations share one guarded implementation and
the existing actor/board/revision/replay transaction. Empty clears the color;
an unchanged value writes no row, revision or request identity. Actual changes
advance only the parent revision, verify the resulting scope/color/version, and
roll back ignored or altered writes. Archived lists reject edits. Both hierarchy
types are tested across palette/hex values, malformed forms, unknown actors,
wrong scope, replay, stale revisions, read-only databases, corruption and reopen.
No HTTP route is added. Native loading/adapters and header/editor integration
remain open in ROADMAP.md.

Strict result readers check SQLite types before requesting text conversion.
SQLite documents that conversion can invalidate later type inspection; BLOB
colors and archive scopes are explicit regression cases. See the official
[SQLite result-value interface](https://www.sqlite.org/c3ref/column_blob.html).
