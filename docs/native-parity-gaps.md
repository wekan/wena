# Native parity gaps and next slices

Source review: 2026-09-07, during the continuation after `0a61868`. This is an
implementation inventory, not a completion certificate or a replacement for
[ROADMAP](../ROADMAP.md). The desktop, descriptions, schema-v3 checklists and trusted font have passed
combined integration gates. Later preference/index changes have their own gates. A module being present does not establish
that the executable exposes it. Final integration and test counts belong in the
roadmap/session report.

The reviewed sources are the existing board components, all native feature
modules, `client/desktop.c`, shared models, migration registry, native platform
adapters and build scripts. Jade areas below use the feature families already
identified by the roadmap; this is not an exhaustive template-by-template
upstream inventory. Source-derived compatibility remains pinned by the existing
WeKan inventories/tests, rather than inferred from similar-looking controls.

## Feature-to-implementation map

| Jade component / feature area | Actual native implementation | Remaining work and prerequisite |
| --- | --- | --- |
| Board header and board menu | `components/boards/board_header.c`, `features/board.c`: title, menu intent and bounded sidebar overlay | Board selection/management and remaining board settings need explicit persistent models and adapters. A board-title editor is not full board settings. |
| Swimlane/list/card layout | `components/boards/board_layout.c`: scoped hierarchy, board-wide lists repeated per lane, bounded geometry and stable group identities | Responsive small-screen behavior, touch, full theme/state comparisons and native drag/drop remain open. |
| Collapse | Integrated bounded list/swimlane state and atomic workspace/actor/board preferences, stale-ID pruning and restart/isolation tests | Card collapse and broader platform execution remain open. Preferences stay separate from shared board mutations. |
| Card title and creation | `features/card_details.c`, `card_create.c`, `card_mutation.c`: bounded inputs and guarded SQLite callbacks | Complete card fields, templates and linked-card semantics. Do not repeat title/create work. |
| Card movement/order | `features/card_move.c`, `card_mutation.c`: parent selection and explicit indexed positioning | Pointer drag/drop should invoke the same guarded adapter; cancellation, scrolling and keyboard alternatives need interaction tests. |
| List/swimlane creation, rename and movement | `hierarchy_title.c`, `hierarchy_mutation.c`, `hierarchy_move.c`, `hierarchy_move_mutation.c`; desktop dispatch exists | Remaining list menus include actions beyond rename/move; archive/delete require child handling and reversible UX before wiring. |
| Archives | `card_archives.c`, shared paginated table and guarded card restore callback; desktop opens panel from Archives section | Native archive panel covers cards, not an archive browser for every hierarchy/entity kind. |
| Card description | Integrated native multiline editor, shared card-version mutation and schema-v2 table; actual SDL save/cancel/reopen regression | Plain text, 0–1024 UTF-8 bytes; Markdown rendering, linked objects and arbitrary-size import remain open. |
| Checklists and checklist items | Integrated schema-v3, complete bounded loader, native create/rename/item add/rename/completion, confirmed deletion and hide-checked/hide-all controls | Reorder, cross-card movement, atomic batch entry and board minicard presentation remain open. Stored minicard inheritance is preserved but has no inactive selector in the UI. |
| Activities | Sidebar renders caller-provided strings and refresh intent | No complete native activity loader/history feed or transactional event emission. Define event semantics and persistence before making Refresh appear functional. |
| Members | Sidebar renders caller-provided strings and Add member intent | No native membership management or board authorization. Local actor selection validates existence, not identity or permissions. |
| Labels | Sidebar renders caller-provided strings and Add label intent; shared color catalog exists | Need board-scoped label records, card-label relationships, guarded mutations and actual picker/minicard display. Theme colors are not persisted labels. |
| Comments | No native comment feature/model in the reviewed component inventory | Separate versioned collection, raw text policy, author/timestamps and edit/delete rules; reuse multiline validation only where semantics match. |
| Attachments | No native attachment feature in the reviewed inventory | Metadata plus bounded file staging/storage policy, ownership, cleanup and preview constraints. A filename field alone is not upload/download parity. |
| Dates, custom fields, subtasks and linked cards | No complete native feature modules in the reviewed inventory | Each needs upstream field/default/scope semantics and persistence before UI. Avoid adding unused schema collections en masse. |
| Search/filter | `features/board_filter.c`: session-only bounded title query; optional leaf predicate preserves model arrays and parent controls | Desktop integration must wire the toolbar and close other card panels on applied changes. Full WeKan search, regex, labels/members/dates remain open. |
| User/login/settings | Local language selection and existing server-settings modules; local desktop accepts actor/board arguments | Authentication, membership authorization and provider parity remain separate server work. Server-setting components do not imply desktop login. |
| Import/export and remote REST | Compatibility inventory, existing server infrastructure and read-only FerretDB probe | No direct WeKan/FerretDB conversion or complete remote REST synchronization. Require source-pinned codecs, version negotiation, backup/rollback and authorization. |

Paths in the table are relative to `client/` unless they explicitly name
`models/`. The sidebar distinction is visible directly in
[`board_sidebar.c`](../client/components/sidebar/board_sidebar.c): it consumes
`WenaSidebarItems` and returns intents; it does not load or write collections.
Desktop routing is owned by [`desktop.c`](../client/desktop.c). New integrations
should continue using this routing and the current adapters.

## Native title-filter contract

The pinned canonical `client/lib/filter.js` defines `StringFilter` using a
case-insensitive regular expression, and `boardHeader.jade` exposes `filter` and
`filter-clear`. The native subset deliberately uses a **literal substring**:
ASCII A–Z folds to a–z, while other UTF-8 bytes compare exactly. Thus `ALP`
matches `Alpha`, but `ä` does not match `Ä`, and `.*` is literal punctuation.
No regex engine, Unicode case-folding or label/member matching is introduced.

Queries accept 0–128 valid UTF-8 bytes and reject malformed encoding and controls.
An overflow sentinel prevents a long query from silently applying a truncated
prefix. Filter/Enter applies; Escape discards the draft; Clear resets applied
and draft text. Invalid drafts retain the previous applied filter. Changing
boards discards the query. Only leaf card rendering is filtered: arrays, order,
list/swimlane geometry and parent mutation scopes are retained. Archived and
foreign-board cards never match. Hidden selected details close; desktop routing
closes its other panels when the applied query changes. New or moved cards are
evaluated from the current snapshot without maintaining a second card cache.

## Further implementation order (after current integrated slices)

1. **Complete remaining checklist actions.** Description, native checklist editing
   and confirmed permanent deletion pass integrated gates. Reordering and cross-card
   movement need complete order/scope transactions. Minicard presentation needs a
   bounded derived board read model, explicit board default and cache invalidation;
   model visibility flags alone do not render it. See ROADMAP for the prioritized
   next Labels assignment.
2. **Board labels and card-label selection.** This makes an existing sidebar
   area functional. Implement a bounded board label loader plus create/rename/
   color operations, then card-label assignment. Derive color/default/delete
   behavior from pinned upstream sources. Test foreign board labels, duplicate
   assignments, stale updates and commit-only display changes.
3. **Integrate and verify session-local card-title filtering.** The bounded
   feature now exists independently of labels. Keep the documented literal
   matching contract, toolbar focus routing and snapshot-preservation tests.
   Additional label/member filtering should follow those collections rather
   than pretending the title subset implements complete search syntax.
4. **Broaden preference/platform coverage.** Scoped collapse preference persistence
   is integrated and POSIX tested. Execute the Windows branch on its target and
   define card-collapse behavior before adding that feature. Do not restart the
   completed file format, pruning, atomic-save and local actor isolation work.
5. **Due-date editing and display.** Pin absent/clear/time-zone/UTC semantics
   before migration. Implement one date field end-to-end, including invalid and
   ambiguous input behavior, before broad calendar or notification work.
6. **Activity history for implemented mutations.** Agree the canonical events
   first. Store events in the same transaction as mutations; replay must not
   duplicate events. Add a bounded read-only sidebar feed and refresh with
   cursor/version semantics. This should not block unrelated UI preferences.

Label persistence, preferences/filtering and UI accessibility can be separate
streams. Description/checklist migrations and common mutation dispatch need one
coordinated owner; splitting edits to the same registry/SQL adapter creates
unnecessary integration risk. Shared validators, existing transaction boundaries
and model scope checks remain the default architecture.

## Keyboard, accessibility and layout limits

Single-line title editors share focused Enter submission and Escape cancellation;
multiline descriptions reserve Enter for a newline. Move/archive panels have
focused Escape behavior. Real Nuklear key and draw-command tests establish these
specific interactions, not full keyboard accessibility. Next checks should cover
Tab/Shift-Tab traversal, visible focus, access to board actions without a mouse,
focus restoration after a panel closes, and screen-reader/platform accessibility
exposure. Native GUI accessibility cannot be inferred from HTML ARIA tests.

The language state records RTL direction, but a direction flag and translated
strings do not implement bidirectional shaping or mirrored geometry. Existing
font work exposes a trusted embedded font helper with selected Latin/Greek/
Cyrillic coverage; it explicitly does not promise universal glyph coverage or
shaping. Verify actual desktop font selection after integration. Arabic/Hebrew
shaping, mixed-direction text and broad CJK coverage remain explicit gaps.

The native light palette and selected real-Nuklear clipping tests are useful
foundations. They do not establish parity for all 25 themes, long translations,
focus/hover/disabled states, mobile viewports or touch targets. Add representative
viewport/state fixtures and canonical references before making visual parity
claims. SDL dummy smoke is startup/render validation, not user-driven visual QA.

## Build and documentation consistency

`build desktop` is the real local SDL2/SQLite application. Cataloged `host` and
cross-target builds remain bootstrap executables. The separate
[`package_desktop.py`](../scripts/package_desktop.py) verifies Linux amd64 only;
its presence does not make Windows, macOS, Android or Amiga desktop packages
complete. Shared host dependencies, provenance and patched SQLite requirements
belong in the dependency audit. Full native tests do not substitute for running
those GUI/platform combinations.

The initial review found the following stale claims. The integration owner has
reconciled the current README, ROADMAP and native desktop guide; this table records
what was corrected, while historical session reports retain their original scope:

| Current text | Required reconciliation |
| --- | --- |
| ROADMAP paused checkpoint starts with list/swimlane movement | Those feature/adapters and desktop dispatch now exist. Advance the checkpoint only after their final integration gates; do not start them again. |
| README desktop summary omits hierarchy reordering and newer detail fields | Add delivered interactions after executable integration, keeping pending modules distinct. |
| `docs/native-desktop.md` calls hierarchy movement unfinished and movement append-only | Describe implemented explicit positions once current tests pass. |
| Desktop docs and checkpoint say schema-v1 only | Once schema-v2 is integrated, distinguish historical v1 compatibility, current bundle target and rejection of downgrade. |
| Desktop docs list descriptions entirely unfinished | Update only when desktop routing, migration and tests establish the complete local bounded slice. |
| Docs describe full feature-message/font work as uniformly absent | Separate canonical message integration, bounded embedded font coverage and still-open shaping/RTL/accessibility work. |
| Different historical suite counts appear in roadmap/session reports | Retain dated history and record one final current result; do not combine counts from different revisions. |

Current test counts and the exact next checkpoint are maintained by the integration
owner in ROADMAP and the latest session report.
