# WeKan - WeKan Native

- Made with C89, SDL2, Nuklear GUI, SQLite.
- One executeable GUI binary, that saves files to wekan-files directory structure like Meteor 3 WeKan FerretDB SQLite
- All code compatible with MIT license. No GPL code.
- Use maintained dependencies with pinned provenance, relevant security updates and
  recorded runtime versions; do not equate a recent version with absence of vulnerabilities.
- Drag drop, looks same like Meteor 3 WeKan.
- For all desktop and mobile operating systems.
- Based on Meteor 3 WeKan https://github.com/wekan/wekan/models
- Local mode: Uses local SQLite database for read and write
- Remote mode: Uses WeKan REST API with any Meteor 3 WeKan URL for read and write
- Import Export between local and remote
- Uses WeKan Jade UI layout, with Nuklear UI components

# Roadmap

## Current continuation (2026-09-24)

Whole-checklist transfer to another active card on the same board is implemented.
The native Rename form offers Move Checklist, an exact-ID destination selector,
and explicit Save/Cancel. One guarded transaction moves every child, including
hidden/completed items, checks both card revisions, preserves content/flags/item
positions, appends the checklist and advances changed revisions. Empty targets,
capacity/position limits, stale/invalid scope, replay, late rollback and reopen
have fast SQLite and real Nuklear/SQLite regression coverage. Individual item
transfer is also implemented: item Edit offers Destination, with exact-ID card
and checklist selectors. Save guards both cards, both checklists and the item,
appends without changing completion, and advances a same-card aggregate only
once. Cancel/Escape, empty destinations, stale selections, collection capacity,
late rollback and failed post-commit refresh have SQLite and real Nuklear tests.
Cross-board transfer now shares these guarded local persistence operations through
an explicit destination-board ID. The same regression bodies run against same-board
and cross-board fixtures, including capacity, malformed scopes, late rollback,
replay, reverse transfer and reopen. Ten focused suites and cross-board ASan/UBSan
checks pass. The desktop now uses a shared paginated board/card destination chooser
for both whole-checklist and item moves. Page reads and selected-card snapshot
reads happen outside drawing. Exact board/card IDs disambiguate duplicate titles;
stale page revisions reject before confirmation. Twenty focused suites and chooser,
directory-reader and directory-picker ASan/UBSan checks pass. Arbitrary insertion
points in drag/drop and full visual parity remain open.

Latest integrated validation: 135 host-independent native suites passed, zero
failed or skipped, after cross-board chooser integration. All four localhost
HTTP/runtime suites previously passed outside the sandbox (runtime rerun after
schema-v9).
The sandbox itself denies loopback bind with EPERM; no listener implementation
change was needed. Fixed a macOS unused-variable build error in OS entropy and a
reference inventory scanner that wrongly excluded checkouts beneath `.tools`.
A detached local reference at pinned WeKan `689a3938` makes both source-parity
checks pass. Desktop checks and targeted preview/presentation/editor sanitizers
pass. Linux desktop packaging remains unverified on this macOS arm64 host; its
existing Linux-amd64-only gate skips here. Other target SDK/platform validation
remains open; continue host-independent implementation and tests.

Latest item-transfer validation: 21 focused native suites passed in 5.25 seconds,
with zero failures or skips; both item-transfer suites passed ASan/UBSan. Desktop
startup, reopen, long-path and negative-input checks pass.

Earlier SVG/whole-checklist validation: 24 focused native suites passed in 5.50 seconds, with zero failures
or skips; SVG, both new transfer suites and the existing ordering suite passed
ASan/UBSan (leak detection disabled on macOS). Local
Apple Clang validation uses a temporary compiler wrapper for Homebrew SQLite
3.53.4 headers/libraries and the pinned Nuklear header's C23-offset diagnostic.
The desktop builds with Homebrew include paths and Darwin feature declarations.
The macOS executable-path bug discovered by startup validation is fixed; startup,
reopen, initialization, long paths and negative-input desktop checks pass. Linux
LD_PRELOAD event injection is not applicable on macOS.

SVG is the authoritative format for UI artwork, theme assets and scaling.
Generate required platform representations from compact vectors rather than
embedding multiple raster sizes. Conversion/rendering code must be MIT or other
copyfree-compatible licensed code; no GPL dependency. Keep editable live text,
input behavior, layout and canonical theme colors, with HTML4 ASCII fallbacks.
The first MIT SVG-to-C89 path is integrated: SVG native-light theme tokens and
board artwork compile to compact vectors, without an XML parser or raster size
variants in the executable. Real Nuklear tests cover scale/aspect/theme color and
invalid-command rejection. Both first-party PNG documentation captures are now
standalone SVG paths with exact RGBA-hash regression checks. Their larger vector
files remain documentation only. The compiler deliberately supports only bounded
rectangles/circles/lines; future complex artwork requires reviewed extensions.
See `imports/ui/svg/README.md`. Full UI/theme/responsive parity remains open.

Reusable components are the implementation policy: shared rendering, pagination,
selection and empty/error states belong in `client/components/`; feature adapters
provide data and action callbacks. Do not duplicate table implementations per page.
The first general paginated table is implemented and used by all four sidebar
item lists. It shares navigation, page clamping, headings, row callbacks, exact
absolute row intents and empty/error handling; translated navigation comes from
the pinned catalog. Real Nuklear coverage includes different adapters and maximal
size_t boundaries. Twenty focused native suites pass; table and sidebar integration
also pass ASan/UBSan. Ordered-card snapshot capture and stale detection now live
in a reusable pure model, used by the Move dialog and ready for card dragging;
archived siblings, exact IDs, numeric bounds and atomic replacement share one
implementation. Nine existing focused suites and the new model test pass.
The table also supports a bounded page cache: missing pages render one translated
loading state and emit a read intent instead of invoking unavailable row data.
A reusable directory picker and SQLite reader serve boards, actors and scoped active cards,
fetching at most 32 validated rows with atomic count/page snapshots. The picker
does no SQL while drawing, selects exact IDs despite duplicate titles and retries
failed reads only on explicit Refresh. The board/card destination component reuses
this picker for cross-board checklist and item transfers in the desktop; wiring
the other host views remains open.
The same drag controller now serves list/swimlane ordering through the existing
Move adapter and pure ordering comparison. It loads the source revision once after
press and commits once after release; drawing stays SQL-free. Eleven focused
suites pass, including desktop and hierarchy movement. Hierarchy-drag and real
board-layout sanitizer checks cover rollback, source/sibling changes, cancellation,
repeated list identities and collapsed-lane control visibility.
Transfer forms now optionally choose an exact insertion ordinal through a shared
bounded numeric control. Both moves use the existing collision-safe order writer,
with one revision advance for the moved row and changed siblings. Default append
behavior remains compatible. Sixteen focused suites and insertion/order/chooser
sanitizers pass. Numeric controls keep Save/Cancel geometry stable when destination
selection becomes invalid and avoid a second popup in the same form. Minicard
checklist/item drag slots now reuse these exact insertion mutations, including
hidden destination sibling ordinals. Eleven focused suites and the expanded real
Nuklear preview sanitizer suite pass.
See `client/components/README.md`.

Expanded minicard contents: the shared board checklist reader now optionally
retains owned ordered checklist/item contents in the same read transaction as its
counts. It uses the same seven statements regardless of card count, allocates only
for existing rows, preserves the previous snapshot on failure, and performs no SQL
for lookups. The desktop now renders checklist titles and visible items after card
controls, independent of compact counts. Explicit checklist Yes/No overrides take
priority over the canonical true default; Default restores inheritance. Titles
open the exact card checklist editor. Real Nuklear/SQLite tests cover visibility,
hidden/completed children, cancellation, reopen and zero-SQL idle frames; targeted
sanitizers pass. Schema-v7 adds the canonical default-on board display preference;
its shared settings panel saves both display booleans atomically, with one board
revision change. Immutable v1-v6 migrations remain unchanged. Upgrade, scope,
stale/replay/no-op, trigger rollback, real Nuklear controls and reopen tests pass.
Inline completion now captures an owned one-shot item intent with all parent/item
revisions and applies the existing guarded mutation after drawing. Failure clears
the intent, shows an error and requires explicit Refresh before more preview
writes; reloads cannot replay it. Eleven focused suites and targeted sanitizers
pass, plus desktop startup checks. Schema-v8 now adds actor/card section
preferences with shared model, storage and collapse control. Minicards and the
opened card share the same checklist key; preferences leave canonical card/board
revisions unchanged. Nineteen focused suites, section/preview/presentation
sanitizers and desktop checks pass. See `docs/section-preferences.md`.
Whole-card collapse uses the same generic preference store and control, retaining
titles/actions and skipping badges/expanded content while folded. Schema v9 adds
the canonical board option to disable folding without discarding actor choices;
the three display settings save in one guarded transaction. Legacy save APIs
preserve the new setting. Migration v1-v8 bytes are unchanged, and a reusable
schema test template covers v7 and v9 upgrades, constraints, rollback and reopen.

Inline checklist/item renaming and single-item creation now share a general
text form, including the existing title editors' focus/keyboard policy. Rendering
captures exact IDs/revisions without SQL; submission uses the existing guarded
mutation after drawing. Validation/stale/transaction failures retain the draft
without automatic replay; Save is explicit, Cancel discards, and disappeared or
hidden targets cancel. Real Nuklear/SQLite tests cover clicks, canonical item
trimming, invalid titles, stale revisions and late transaction rollback.
Batch entry now uses one checklist-entry adapter in both opened cards and
minicards, built on the general single/multiline text form. The canonical newline
toggle preserves incompatible drafts; Enter inserts a line in batch mode and
only Save submits. Shared parsing enforces eight items, canonical trimming,
insertion order and atomic failure. Twenty-one focused suites and both entry
sanitizer suites pass. A reusable same-collection drag handle now reorders
minicard checklists and checklist items through their existing guarded mutation.
It captures a seven-pixel gesture, exact IDs, scope and revision, then consumes
the intent after drawing. Escape, hidden sources, stale collections, outside drops
and read-only controls cancel. Hidden siblings retain their true ordinals.
Twenty-three focused suites, both drag/preview sanitizer suites and desktop checks
pass. Explicit destination zones now reuse the gesture control for same-board
whole-checklist and item transfers, including cross-card item moves. Destination
revisions, late rollback, no replay and completed-item preservation have real
Nuklear/SQLite coverage. Twenty-four focused suites pass. Arbitrary insertion
points, cross-board transfers and full drag/drop parity remain open.

Blocker procedure: inspect original WeKan behavior and pinned dependency source,
then consult official documentation and copyfree-compatible implementation examples.
Record unresolved constraints here with the attempted resolution and affected work;
continue all independent work. The current contents projection has no unresolved
blocker. SQLite's documented read-transaction snapshot semantics and the existing
validated streaming queries support the implementation without a new dependency.

## Earlier checkpoint (2026-09-19; historical)

**Current continuation (2026-09-11).** Labels/card assignments, atomic checklist
batch input, same-card checklist/item reordering, compact count presentation and
schema-v6 board display settings are integrated locally. Continue from cross-card
checklist movement and expanded minicard presentation; do not restart these
completed slices.

Focused validation covers schema-v6 upgrade/interruption/restore, board
presentation's zero-SQL idle frames and failed-refresh recovery, real Nuklear
settings/order panels, exact cached label/count badges, and an actual SDL desktop
workflow for settings opt-in, Cancel, 0/0 count display and restart. Native title
and description editors retain a complete over-limit four-byte UTF-8 scalar in
their draft buffers while applying the stricter persisted-model bounds on Save;
the focused regressions cover the resulting reject-without-write behavior. Both
the broad normal and complete ASan/UBSan gates have passed for this checkpoint.

Current decisions: group new label code under `client/features/labels/` and typed
operations under `server/mutations/`; reuse the existing guarded transaction and
request registry. One `models/color` table supplies both the legacy UI contract
and labels, and `models/text` supplies strict UTF-8 ECMAScript trim to both label
and checklist inputs. Label names may be empty, item colors follow the pinned
25-color palette or exact `#RRGGBB`. All real label changes advance the board
version, including card assignments, so captured deletion confirmations detect
intervening assignments; this deliberately trades more board contention for a
single reliable aggregate guard. Deletion removes assignments from archived
cards too. Batch entry stays within the existing 4097-byte request envelope:
at most eight 128-byte titles, atomically appended. Reordering guards the complete
card collection and compacts positions atomically. Count presentation uses a
bounded snapshot rather than render-frame SQL; a missing `board_settings` row
means the canonical false default. Cross-card movement and expandable minicard
contents remain open.

The original title-edit checkpoint and the following Add card checkpoint have
been completed locally from upstream `51f8ad1` and local checkpoint `04afa15`.
Do not repeat migration hardening, native title/archive/create/move/restore card
adapters, bounded board loading, hierarchy creation/renaming, workspace seeding,
runtime label lookup/language selection, or swimlane/list collapse. Their tested
implementations are present below.

List/swimlane reordering, indexed card movement, descriptions, checklist
create/edit/complete/delete, literal filtering and persisted collapse preferences
are implemented and integrated; do not restart them. The current artifact embeds
the verified schema-v6 chain and the measured checklist query index.

The next native feature slice is **cross-card checklist movement and fuller
minicard presentation**. Labels, card-label assignment, atomic batch entry,
same-card reordering and default-off compact counts are complete locally. Keep
`build desktop`, real input and the full suite running while progressing through
the component inventory.

Current executable boundary: `build desktop` is a tested POSIX SDL2/SQLite
application that creates/reopens a local Wena schema-v6 workspace and upgrades
older Wena schemas atomically. It supports card creation/title/description,
movement/archive/restoration, hierarchy creation/renaming/reordering, checklist
create/edit/complete/confirmed deletion, persisted collapse, literal filtering and
canonical language choice.
Cataloged cross-release targets still build the earlier bootstrap executable.
Remote REST, WeKan/FerretDB conversion, complete collections, full fonts/RTL and
UI/platform parity remain open. Local actor selection trusts the OS user;
it is not login or board-membership authorization. See `docs/native-desktop.md`.

Latest validation (2026-09-19): **123 native suites passed, zero failed or
skipped**, followed by all **84 ASan/UBSan groups** (run in bounded groups using
the pinned SQLite 3.51.3 headers/libraries). Explicit host bootstrap build, real
SDL workflows and verified Linux desktop packaging passed. LeakSanitizer is
disabled on this host. Earlier reports remain historical. Do not interpret host
SDL/SQLite or JavaScript VM coverage as cross-platform GUI release validation or
real-browser E2E.
No GitHub write, push, PR, release or tag was performed.

## Coordinated continuation after `0a61868`

Work is split into independently owned implementation streams. A completed
subtask receives the next useful slice; shared-file changes are coordinated and
the primary agent owns desktop integration and this roadmap. A slice is checked
only after its implementation, targeted regressions and integration support it.

| Stream | Current slice | State |
| --- | --- | --- |
| Native UI | Labels, board setting and compact badges | Integrated; actual SDL opt-in/Cancel/0/0/restart flow passes |
| Persistence | Assignments, board setting and cached presentation | Integrated; idle SQL, rollback, retry and torn-read gates pass |
| Shared models/review | Trim, colors, versions and owned clipboard handling | Focused C89/UTF-8/selection regressions pass |
| Checklist workflows | Batch input, same-card ordering and summaries | Integrated; cross-card movement/expanded minicard contents remain open |
| Build/tests | Native, SDL and sanitizer regression catalogue | Focused gates pass; broad rerun is the current verification step |
| Storage evolution | Immutable schema-v1–v6 | V6 settings upgrade/restore/interruption/concurrency gates pass |

Each stream receives the next bounded task after completing a slice. Tests use
verified SQLite 3.51.3 for native C work. The baseline results above remain
historical until all current integrations are rerun; no broad WeKan/platform goal
is inferred complete from focused gates.

Architecture decisions for this cycle:

- Keep future board display settings in one additive `board_settings` extension
  table, with the existing board version as its optimistic boundary. A missing
  row means the canonical false default. This follows the description extension
  pattern, preserves immutable original table definitions and reuses the audited
  CREATE-only migration registry instead of adding an ALTER execution path.
- Render labels from a complete bounded bitset snapshot, never per-frame SQL.
  Invalidate cached presentation after commit; a reload failure exposes an
  explicit read-only Refresh and must not replay a successful mutation.
- Keep the pinned Nuklear source unchanged and install an owned SDL clipboard
  paste callback. It validates the complete UTF-8 payload, filter, selection
  and byte capacity before the length-delimited editor call, because the pinned
  generic paste path treats byte length as a rune count. Rejected pastes leave
  both draft and selection unchanged.

- Reuse the existing model module for strict ID/title validation. Keep its
  permissive string-copy API and different multiline-region/path contracts
  distinct; consolidation must not silently change accepted data or limits.
- Guard native hierarchy movement against concurrent sibling reordering as well
  as the selected object's version. A selected-row version alone does not detect
  every change to the order displayed by the client.
- Keep desktop packaging separate from the bootstrap release catalog. Declare
  actual dynamic host requirements and verify the extracted executable; a native
  Linux build does not prove GUI support on other platforms.
- Establish one compiled migration registry and shared schema-history checks
  before adding fields/collections. Retain the reviewed v1 migration bytes and
  compiled-SQL-only execution gate; descriptions use the tested atomic schema-v2
  transition, and checklists use the separately tested additive schema-v3.
- The dependency audit found SQLite's documented WAL-reset race, relevant to
  Wena's concurrent WAL connections. Test and build validation now use official
  SQLite 3.51.3 source verified against its published hash, outside this repo.
  Deployment must use the fix or a confirmed distribution backport; version
  strings alone cannot establish whether an older distribution build is patched.
  See `docs/dependency-audit.md` and the SQLite
  [WAL-reset advisory](https://sqlite.org/wal.html#walresetbug).
- Centralize native editor focus and defer newly opened panels until the next
  input frame. A mouse release that opens a panel must never also activate a
  control inside that panel; the actual SDL transition regression caught this.
- Treat a card and its checklist collection as one optimistic boundary: every
  checklist mutation checks/increments the card version. Changed checklist/item
  rows also advance their own versions; counts remain derived, avoiding a second
  stored source of truth. Explicit capacities and failure-before-publication
  apply to complete snapshots. Confirmed permanent deletion is now implemented;
  cross-card movement, activities/undo retention and REST remain separate.
- Add only the measured four-column selected-card item index, using a new
  schema-v4 migration. Keep the query unchanged so malformed selected-card rows
  are still returned for validation. A narrower index left temporary sorting;
  the complete order index eliminates scans/sorts in the documented C fixture.
- Embed a trusted, commit/hash-pinned Apache-2.0 Roboto font instead of adding a
  runtime font-file parser path or conversion dependency. Selected Latin, Greek
  and Cyrillic coverage is tested; CJK, Arabic shaping and RTL layout remain open.

## Expanded build, release, test, and server phases

- [x] Add `config/targets.tsv` as the shared catalog of realistic GitHub Actions
  cross-build targets, distinguishing verified bootstrap targets from planned ones.
- [_] Expand cross-build coverage one verified target at a time:
  - [_] Linux i686, ppc64le, s390x, and riscv64.
  - [_] Windows i686 and arm64.
  - [_] Android armv7, x86, and x86-64.
  - [_] iOS Simulator arm64 and amd64.
  - [_] FreeBSD, NetBSD, and OpenBSD on amd64 and arm64, using pinned sysroots.
  - [_] Haiku amd64 with its maintained cross-tools.
  - [_] WebAssembly wasm32 as a self-contained web artifact bundle.
  - [_] Replace the old AROS SDK image when a maintained compatible image or
    reproducible current-source toolchain is available.
- [x] Make `.github/workflows/release-all.yml` a complete Wena release workflow:
  - [x] Require an existing newest `github.com/wekan/wena` release and resolve its
    tag without creating, publishing, editing, or pushing a release.
  - [x] Build every `ready` catalog target; a missing script or artifact is a hard
    failure rather than a skipped target.
  - [x] Collect every verified executable/bundle under unique release asset names.
  - [x] Attach all collected assets to that newest release with `contents: write`,
    collision handling, post-upload verification, timeouts, and per-job summaries.
  - [x] Add static regression tests for permissions, dependencies, complete asset
    coverage, and the no-release-creation/no-push boundary. Implement/test only;
    never run this workflow or invoke a release/upload command from an assistant.
- [x] Add WeKan-style local build entry points:
  - [x] `build.sh` with Build, Tests, Server, and Tools submenus plus noninteractive
    `--list` and named commands.
  - [x] `build.bat` with the same categories, target names, and exit codes.
  - [x] Build the current host target, one selected catalog target, or every ready
    target; long non-menu commands return directly to the prompt without pauses.
  - [x] Share target dispatch/validation between menus and CI to prevent drift.
  - [x] Add one deterministic native test catalog and `tests all`: bounded parallel
    independent suites, serialized shared builds, Python interpreter dispatch,
    per-suite timeout/error reporting and failure exit status. Missing optional
    prerequisites are explicit skips; target-specific release tests stay explicit.
    Source parity accepts a separately located pinned checkout via `WEKAN_ROOT`.
  - [x] Add a separate `build desktop` host command without relabeling bootstrap
    cross-platform catalog targets as complete GUI releases.
- [_] Embed every canonical `wekan/imports/i18n/data/*.i18n.json` translation in
  every one-file Wena executable/artifact; semantic equivalents of Meteor WeKan
  pages and actions use the same WeKan keys and values, never a parallel catalog:
  - [x] Generate one deterministic, compact offline catalog from the canonical
    UTF-8 JSON files, rejecting missing/reordered English keys and changed
    underscore/printf placeholder inventories; record source revision and hashes.
  - [x] Pin the canonical WeKan revision, commit its generated catalog, document
    MIT provenance/size budget, and make local/release builds fail when regeneration
    differs, a language is absent, or the embedded catalog marker/hash is missing.
  - [x] Add a strict-C89 runtime reader and link the same catalog into every ready
    target (and every future target before it becomes `ready`) without network use.
  - [x] Normalize OS locale identifiers deterministically (`language_REGION`,
    `language-Region`, encodings, and modifiers) and resolve exact variant, then
    base language, then English; support Windows, macOS/iOS, POSIX Linux/BSD,
    Android, AmigaOS, and AROS locale APIs with explicit capability fallbacks.
  - [x] Persist an explicit user language that overrides first-run OS detection,
    and support immediate runtime language switching including RTL direction.
  - [_] Map implemented Wena views/actions to canonical WeKan i18n keys and add
    parity tests for all languages, key order, placeholders, UTF-8/RTL, missing
    keys, locale normalization/fallback, runtime switching, and offline binaries.
    - [x] Generate a strict-C89 static lookup for implemented shared-contract keys
      directly from the pinned full catalog, for all 246 languages. Compare every
      compiled value with canonical bytes; verify placeholders, fallback, exact
      underscore/modifier tags and deterministic generation without new dependencies.
    - [x] Connect runtime labels, an SDL toolbar language chooser and an atomic
      per-workspace preference. Test immediate switching, every canonical tag,
      invalid/read-only paths, write failure/recovery and bounded long CLI paths.
      Current feature messages use canonical keys. Selected embedded Latin/Greek/
      Cyrillic glyphs are tested; full font coverage and RTL layout remain open.
- [_] Add fast native equivalents of WeKan test categories, running against the
  current OS/CPU executable wherever behavior crosses a process boundary:
  - [x] Strict-C89 model/unit and negative-validation suites.
  - [_] Nuklear component, interaction-state, accessibility, keyboard, mouse,
    touch, responsive-layout, drag/drop, and collapse suites.
    - [x] Add real Nuklear mouse/text-input Save/Cancel/bounds regression coverage
      alongside fake component/state and SQLite integration suites.
    - [x] Verify actual board draw-command/scissor visibility, readable list-column
      geometry, real mouse collapse/expand, shared lists and duplicate-title lanes.
      Fix clipped inherited group heights with explicit scrollable lane/list rows;
      complete responsive/theme/accessibility parity remains pending.
  - [_] Headless SDL executable smoke/startup/crash and command-line suites.
    - [x] Build and execute the optional desktop using the SDL dummy video driver;
      verify bounded successful startup, malformed/duplicate arguments, missing or
      corrupt files, unknown actor/board, and logical database immutability.
    - [x] Exercise explicit workspace initialization, localized seed titles,
      no-overwrite and long database paths through the real executable. On Linux,
      inject real SDL events from a test-only preload helper and verify that a
      toolbar action opened over the sidebar creates a persisted list.
    - [x] Preserve every UTF-8 code point in complete SDL text events through the
      existing Wena platform adapter. Reject malformed/control/unterminated events
      and frame-capacity overflow without partial insertion; test real SDL input,
      multi-event frames and the integrated non-ASCII list creation workflow.
  - [_] SQLite schema, migration, transaction, corruption, concurrency, and query
    performance suites using temporary databases.
    - [x] Close code-scanning alert #1 (`cpp/sql-injection`): accept only the exact
      compiled schema-v1 migration bytes, execute only the compiled SQL literal,
      bind migration metadata, and reject altered payload/hash combinations before
      opening a transaction; cover valid, tampered, substituted-hash, and restart
      paths with strict-C89 tests.
  - [_] REST contract/authentication/authorization/rate-limit and negative suites.
  - [_] WeKan/Trello import-export round trips, malformed input, attachment paths,
    and local/remote boundary suites.
  - [_] Platform artifact-format, dependency/license, sanitizer, fuzz, leak, and
    performance regression suites; run independent native suites in parallel.
    - [x] Add an opt-in ASan/UBSan runner for native model/UI/storage regressions.
      Leak detection defaults on; report when the host cannot support it. Full
      fuzzing, leak validation and performance/platform coverage remain pending.
- [_] Add optional Wena Server in Admin Panel / Settings / Server:
  - [x] Configuration model/UI: disabled by default; explicit IPv4 bind address and
    validated port (for example `127.0.0.1:3000`), with restart/status/error state.
  - [_] Server adapter with bounded HTTP parsing, connection/request limits,
    timeouts, authentication tokens, authorization, audit logging, and safe CORS.
    - [x] Add a fail-closed bounded HTTP/1.x request parser and fixed connection,
      per-connection request, body/header, and timeout limits; add an IPv4-only
      listener start/stop/restart lifecycle driven exclusively by validated Admin
      settings, without accepting or dispatching mutations yet.
    - [x] Add bounded opaque auth sessions and route/operation/session-scoped,
      single-use CSRF tokens sourced through a required entropy adapter; audit
      accept/reject/expiry/replay/scope/capacity decisions without storing secrets.
      Mutation dispatch remains closed until these controls are integrated.
    - [x] Add production OS cryptographic entropy (Windows CNG, Apple/BSD
      `arc4random_buf`, Linux/Android `getrandom`, fail-closed `/dev/urandom`
      fallback) and strict same-origin CORS plus no-store/nosniff/frame/referrer/
      CSP response headers; unsupported platforms cannot enable authenticated mode.
    - [x] Add a one-request-per-connection serving loop with bounded accept/read/write
      timeouts and parser buffers: shared-contract GET renders HTML4, while every
      POST remains closed with no security-token consumption or mutation intent.
  - [_] Versioned WeKan-compatible REST routes for users, boards, swimlanes, lists,
    cards, checklists, comments, labels, members, attachments, and activities.
  - [_] Back routes with the same SQLite model/storage layer used by local mode and
    preserve transactional parent relationships.
    - [x] Pin and execute the version-1 SQLite schema golden for actors/sessions and
      board/swimlane/list/card parent hierarchies, deterministic position indexes,
      optimistic row versions, and actor+route+operation+request-version idempotency.
      Document forward-only atomic checksum migrations, WAL/foreign-key/integrity
      startup gates, crash recovery, and verified atomic backup/restore. The SQLite
      migration runner and production adapter are implemented in subsequent slices below.
    - [x] Add the smallest strict-C89 system-SQLite migration runner with a bundled
      MIT-compatible SHA-256 verifier: reject modified SQL before open, apply version 1 with
      `BEGIN IMMEDIATE`, checksum metadata and `user_version` in one transaction,
      refuse gaps/downgrades, configure busy timeout/WAL/full sync/foreign keys, and
      gate startup on quick/FK checks with a full integrity API. Temp-db tests cover
      idempotent reopen, uncommitted crash rollback, bad SQL rollback, modified hash,
      newer schema, corruption, and the SHA-256 known vector. Migration embedding,
      backup execution, and production persistence are covered by later slices below.
    - [x] Pin migration bytes, size, schema version, and SHA-256 in a reviewed lock;
      fail every local/CI target before compile when it is stale. Append the exact
      migration plus a checksummed length footer to every ready single-file artifact
      before the final i18n payload, and extract/compare it from a host executable.
      Runtime lookup from the executable and the SQLite transaction adapter remain.
    - [x] Add a production SQLite create/edit/archive callback with `BEGIN IMMEDIATE`,
      actor/board/FK authorization, exact idempotency tuple, optimistic card versions,
      complete region validation, and mutation+idempotency commit in one transaction.
      Temp-db tests cover success, replay, conflict rollback and restart persistence;
      subsequent slices below add listener registration and hierarchy mutations.
      Idempotency metadata stores the actual SHA-256 of the fully encoded region
      response, and commit requires exactly one pending-to-checksummed transition.
    - [x] Decode native/form titles with bounded percent/plus decoding and reject
      duplicate fields, malformed escapes, NUL and ASCII controls. Parse every
      optimistic version/position with exact overflow-checked decimal rules;
      validate request-version range and bounded command strings before beginning
      a transaction. The original nine operations have malformed-numeric, rollback,
      response-clearing and idempotency-key isolation regression coverage.
    - [x] Create verified SQLite online backups without stopping the listener: preflight
      source integrity/FKs and bounded free space, snapshot through SQLite's backup API,
      recheck schema/integrity/FKs, stream SHA-256 into a sidecar, then publish data and
      checksum from deterministic temporary names. Refuse overwrite, stale temp files,
      relative paths, insufficient space, and partial publication.
    - [x] Restore only a sidecar-verified, schema-version-1, migration-checksum-matched,
      integrity/FK-clean snapshot after bounded disk-space preflight. Copy to a
      deterministic staging file before listener stop, atomically preserve/swap the
      original, reopen/restart through a lifecycle adapter, and restore plus restart
      the original on swap or reopen failure. Refuse stale recovery files, corruption,
      truncation, wrong checksums and newer schemas before disrupting the listener.
    - [x] Connect backup and restore to bounded Admin Settings feature actions with
      idle/busy/success/error state and a fixed-capacity, secret-free event audit.
      Backup requires a running managed database; restore loads the executable's
      pinned migration, delegates listener stop/swap/reopen/restart to the verified
      lifecycle, reports rollback distinctly, rejects re-entry, and never records
      database paths, URLs, session material, or detailed storage errors.
    - [x] Add the first hierarchy mutation beyond cards: an explicit
      `edit-board-title` domain allowlist value backed by the shared SQLite adapter.
      Require an authenticated actor, route-owned board, nonempty bounded title,
      optimistic version and exact idempotency tuple inside one transaction; conflict,
      unknown actor/board and replay leave both board and metadata unchanged. List and
      swimlane operations remain separate follow-up slices.
    - [x] Add `edit-list-title` through the same typed dispatch and transaction
      boundary. Bind the list to the route-owned board, require actor, bounded title,
      optimistic version and idempotency metadata, and roll back cross-board,
      conflict, replay and malformed requests without partial hierarchy changes.
    - [x] Add `edit-swimlane-title` as the matching final hierarchy-title slice,
      preserving route-board parent ownership, actor authorization, bounded inputs,
      optimistic versioning, exact idempotency and atomic rollback. Cover success,
      conflict, replay, cross-board rejection and persistence across reopen.
    - [x] Add a managed server runtime that opens/checks SQLite, registers persistence
      and domain adapters, then starts the listener; stop reverses that order and all
      partial-start failures close the database. Successful no-JS mutations use a
      server-owned version and return 303 only to ROOT_URL plus the validated current
      route; enhancement mutations keep the negotiated V1 response path.
    - [x] Locate the migration directly from the running single-file artifact by
      walking backward over the final i18n footer, then validate SQL footer magic,
      bounded lengths and SHA-256 before DB open. Missing, truncated and corrupt
      artifacts fail closed and free buffers; runtime exposes start-from-executable.
    - [x] Connect Admin Settings submit/toggle/restart to the managed runtime: validate
      the complete disabled-default configuration first, stop an old runtime before
      restart, load migration from the executable, and expose only a generic startup
      error without paths, SQL, tokens, or database details.
    - [x] Add bounded executable discovery adapters: Windows Unicode module path,
      Apple executable path, Linux `/proc/self/exe`, BSD procfs capability, and
      fail-closed Amiga/AROS defaults with an explicit mockable capability boundary.
      Require absolute, exact-length, NUL-terminated valid UTF-8 and reject relative,
      embedded-NUL, invalid-Unicode, truncated and overflow results.
    - [x] Define a strict-C89 begin/apply/finish persistence transaction contract and
      an in-memory create/edit/archive fake. Stage all writes, authorize first, bind
      replay keys to actor+route+operation+request-version, require optimistic card
      versions, validate the complete bounded region response before commit, and
      rollback callback/validation/conflict failures without partial output. SQLite
      now uses the separately versioned schema and adapter described above.
  - [_] Let remote clients select either a Meteor 3 WeKan base URL or Wena Server
    base URL, with capability/version discovery and compatible error handling.
  - [_] Import/export and local-to-remote/remote-to-local round-trip tests against
    both server implementations, including auth failures and interrupted transfers.
  - [_] At configured `ROOT_URL` (scheme, host, port, and optional base path), render
    the same semantic accessible Legacy HTML4 Kanban pages and POST-button actions
    on the same WeKan URL families; generate every link/form action below ROOT_URL
    while listening only on the Admin-configured IPv4 address and port.
    - [x] Add fail-closed ROOT_URL path joining plus an escaped strict HTML 4.01
      one-content-table baseline and token-required POST form rendering from the
      shared contract; listener/route dispatch remains a later server slice.
    - [x] Dispatch shared-contract GET routes as read-only page intents and gate
      known board POST operations behind authenticated, exact-route/operation
      single-use CSRF before returning mutation intents; no storage mutation or
      domain callback is connected yet.
  - [x] Share one page/component contract with Meteor HTML4: canonical WeKan i18n
    keys/values, colors, one content-table baseline, ASCII controls, natural tab
    order, and REST-domain operations; do not create another visual/text catalog.
  - [_] Protect HTML4 sessions and mutations with scoped CSRF tokens, replay
    prevention, authorization, safe redirects, output escaping, and GET immutability.
    - [x] Connect the bounded listener POST gate to the audited session and exact
      route/operation/single-use-CSRF verifier with an injected monotonic clock.
      Valid no-JS forms reach a non-mutating 503 intent boundary; invalid auth,
      scope, media type, operation, and replay fail closed as 403 over real sockets.
    - [x] Add a strict-C89 domain-operation adapter that maps only allowlisted typed
      operations from a verified mutation intent into an owned bounded command for
      an explicitly registered callback. Require monotonic request versions and a
      valid bounded `WENA-REGIONS/1` result; invalid/auth/replay/callback failures
      have no adapter-side version advance. Only a fake callback is tested: no
      SQLite, listener dispatch, or production persistence is connected yet.
    - [x] Connect that adapter optionally to the listener, null by default. Only an
      exact V1 Accept header paired with a bounded positive request-version reaches
      it after the security gate; encode the callback result as the exact regions
      media type. Missing adapter, malformed negotiation/version, replay, and callback
      errors fail closed. Ordinary no-JS POST stays on its separate non-mutating 503
      path until a persistence-backed HTML result/redirect transaction exists.
  - [_] Add Meteor HTML4 route-parity and golden/contract tests, cookieless/no-JS
    HTTP E2E, forged-scope/CSRF/replay/open-redirect/escaping negative tests, and
    configured startup/restart/listener tests.
    - [x] Pin the Meteor Legacy HTML4 route-family golden with its source revision
      and require exact ordered parity from Wena's shared page contract. Exercise
      cookieless GET plus authenticated valid, wrong-session, wrong-operation,
      wrong-route, and replayed no-JS POSTs through the real IPv4 listener.
  - [_] Keep visible HTML4 move buttons as the always-working no-JS baseline.
    After a capability script proves JavaScript and required drag/drop APIs work,
    hide only equivalent card/list/swimlane move controls and expose minimal drag/drop
    with the same session, route, operation, and single-use-CSRF POST semantics;
    never branch on User-Agent, and restore baseline controls on any failure.
    - [x] Serve a small same-origin external capability script under ROOT_URL. It
      performs a real synthetic DataTransfer/DragEvent probe, never reads User-Agent,
      hides only paired move baseline controls, transfers focus and keyboard/ARIA
      semantics, and restores controls on failure or bounded-operation timeout.
    - [x] Bind drag sources and keyboard-accessible drop targets only after that probe,
      and only to an existing ID-addressed baseline POST form containing nonempty
      session, operation, and CSRF fields. Drag data is a constant capability marker,
      never an object ID or operation; the target form alone supplies semantics to the
      bounded transport. Failure restores every baseline control and live status.
      Persistence is now connected only through the later verified hierarchy moves.
    - [x] Render stable-ID move controls as a visible, ordinary HTML4 POST form with
      session, scoped one-use CSRF, and operation fields plus an initially hidden
      drag control bound back to that exact form. A same-origin external stylesheet
      hides enhancement controls without JavaScript; capability success alone swaps
      visibility. Cover the real listener's cookieless/no-JS GET and asset paths.
    - [x] Close the shared-contract gap for card/list/swimlane movement: define the
      three canonical POST operation and i18n keys in the common UI control table so
      the router can authorize exactly the operations rendered by HTML4 and enhanced
      DnD. Exercise visible signed baseline rendering for all hierarchy types; the
      later card/list/swimlane slices provide their SQLite position transactions.
    - [x] Persist the first admitted movement intent, `move-card`, atomically: accept
      object and destination IDs only from the verified baseline form, require the
      card plus target list/swimlane to share the route board, reject archived cards,
      enforce optimistic version/idempotency, and append at a collision-free target
      position in the same SQLite transaction. Invalid parents, conflicts and replay
      roll back without movement or metadata; list/swimlane reordering remains pending.
    - [x] Persist `move-list` from the same signed HTML4/DnD POST contract. Validate
      actor, route-board parent, list ID, bounded target position and optimistic
      version before shifting the board's unique contiguous positions atomically;
      record the exact idempotency response in that transaction. Cover success,
      forged parent, conflict, replay, reopen persistence and the no-JS baseline.
    - [x] Persist `move-swimlane` with the identical signed baseline/DnD operation
      boundary and collision-free contiguous reorder transaction. Validate actor,
      route-board scope, target bounds and optimistic version before writes; commit
      reorder, response checksum and idempotency together. Test success, forged scope,
      conflict, replay, reopen persistence and progressive/no-JS regressions.
  - [_] Define a bounded same-origin enhancement response containing operation result
    plus versioned replacements only for named regions already visible on the page.
    Validate region names/schema/size and stale or out-of-order versions client-side;
    never execute returned script or accept unknown targets, and cover replay,
    abort/timeout, focus, keyboard/accessibility, CSP, and no-JS negative behavior.
    - [x] Define and parse `WENA-REGIONS/1`, an atomic text-only first version with
      explicit byte lengths, strict UTF-8, 32 KiB/8-region/4 KiB-region limits,
      allowlisted visible-region names, monotonically increasing request/region
      versions, partial updates, and replay/stale/out-of-order rejection. HTML is
      intentionally absent until a separately tested sanitizer/schema version.
    - [x] Add the matching browser V1 byte parser and atomic applicator: fatal UTF-8,
      identical schema/name/count/size/version checks, current-visible-ID lookup,
      validate-all-before-write, `textContent` only, and baseline restoration on
      unknown/malformed/stale/out-of-order data. Fetch/drop remain disconnected.
    - [x] Add a disconnected baseline-form POST transport primitive: exact same-origin
      action and POST checks, existing form fields, same-origin credentials, strict
      media type/response limit, monotonic request IDs, one in-flight request, and
      AbortController timeout/failure restoration. The later guarded DnD binding and
      hierarchy persistence slices now invoke it through signed baseline forms.
    - [x] Discover every currently visible allowlisted region from renderer-owned
      stable IDs and versions instead of hard-coding the board. The integration suite
      combines no-JS HTML/socket fallback, capability-supported/unsupported and POST
      failure contracts, and strict-C behavioral malformed/oversized/stale/replay plus
      atomic multi-region/partial-update tests. A real browser-runtime E2E remains
      pending because the build environment provides no browser or JavaScript engine.
    - [x] Complete the server-side progressive movement audit: card, list and
      swimlane baseline/enhanced intents now reach typed, board-scoped, optimistic,
      idempotent SQLite transactions, while the same V1 response can update every
      allowlisted currently visible region atomically. The no-JS form/303 path and
      capability/POST/parse/apply failure restoration remain covered together.
    - [x] Execute the actual C-emitted enhancement script in a dependency-free
      Node VM/DOM harness. Cover parsing/UTF-8/bounds/stale/atomic text updates,
      capability restoration, focus, signed POST, abort/error/replay/retry and
      keyboard/pointer movement. Fix late timed-out responses updating UI and
      synchronous transport failures leaving in-flight state locked. This is
      JavaScript runtime integration; a real-browser E2E remains pending.
    - [x] Document the canonical dependency-free progressive protocol, including
      signed HTML4 authority, capability activation, exact V1 framing and limits,
      safe text-only DOM application, CSRF/authz/idempotency boundaries, focus and
      keyboard behavior, failure restoration, and current test/E2E limits in
      `docs/progressive-enhancement.md`.
    - [x] Make the renderer's visible baseline form the complete movement payload
      authority: emit allowlisted object/destination IDs, optimistic version, and
      bounded target position as hidden fields alongside session/CSRF/operation.
      Reject mismatched operation schemas and unsafe IDs before rendering, so no-JS
      submit and enhanced DnD post identical validated fields.
    - [x] Render the enhancement's single stable status target as an empty polite,
      atomic ARIA live region in the original HTML4 page. DnD selection, success and
      failure messages are therefore announced without stealing focus; no-JS pages
      retain an inert empty paragraph and their natural button/tab order.

- [x] Add GitHub Actions release-all.yml that crosscompiles for many operating systems
  - Added the target matrix, runner selection, per-target build-script contract,
    artifact upload, and a structural regression test. Targets remain pending until
    their cross-compilation scripts produce verified binaries.
- Operating systems at the beginning at release-all.yml:
  - [x] Linux arm64
    - Builds a strict C89 ARM64 ELF executable with GCC and verifies its ELF class
      and AArch64 machine header before artifact upload.
  - [x] Linux amd64
    - Builds a strict C89 x86-64 ELF executable with GCC and verifies its ELF class
      and AMD64 machine header before artifact upload.
  - [x] Linux armhf
    - Builds a strict C89 32-bit ARM hard-float ELF executable and verifies its
      ELF class, ARM machine header, and hard-float ABI flag before artifact upload.
  - [x] Windows amd64
    - Cross-builds a strict C89 PE32+ executable with MinGW-w64 and verifies its
      x86-64 COFF architecture before artifact upload.
  - [x] macOS arm64
    - Builds a strict C89 Mach-O executable with Apple's arm64 target and verifies
      both the Mach-O format and the single arm64 architecture before upload.
  - [x] macOS amd64
    - Builds a strict C89 Mach-O executable with Apple's x86-64 target and verifies
      both the Mach-O format and the single x86-64 architecture before upload.
  - [x] AmigaOS 3.x m68k
    - Cross-builds strict C89 for the baseline Motorola 68000 with the maintained
      AmigaDev GCC 10 container, then verifies Amiga HUNK format and magic bytes.
  - [x] AROS x86
    - Cross-builds strict C89 for the AROS x86-64 ABI with a digest-pinned AROS
      SDK, verifies the compiler target triplet, and validates the x86-64 ELF output.
  - [x] Android arm64
    - Cross-builds a strict C89 AArch64 native executable with stable Android NDK
      r29 for API 21+, then verifies the architecture and Android linker path.
  - [x] iOS arm64
    - Cross-builds an unsigned strict C89 arm64 Mach-O executable for iOS 13+ and
      verifies its architecture and iOS build-version platform before upload.
- [_] Using same directory structure like Meteor 3 WeKan, save files as C89 and Nuklear GUI code
  - [x] Establish documented `client/components`, `client/features`, `models`,
    `imports`, and `server` source boundaries and move the C89 entry point to client.
  - [x] Add shared C89 model modules matching the core Meteor WeKan board,
    swimlane, list, and card collections, with bounded strings and relationship
    validation. Remaining collections will be added as their features are ported.
  - [_] Add Nuklear component and feature modules matching the Jade UI areas.
    - [x] Add the first board feature and component slice, rendering active
      board/swimlane/list/card hierarchy through a strict C89 Nuklear interface.
    - [x] Pin upstream Nuklear as a submodule at commit `e3e18dc1`, select its
      MIT license, and compile its SDL2 renderer behind the platform boundary.
    - [_] Port the remaining Jade component and feature areas.
      - [x] Port the board header title and board-menu action as a dedicated
        Nuklear component, separate from board hierarchy rendering.
      - [x] Port board sidebar visibility and Activities, Members, Labels, and
        Archives section state, integrated with the board-header menu action.
      - [x] Render caller-owned content for Activities, Members, Labels, and
        Archives, with refresh, add, and restore action intents and state checks.
      - [x] Port the list header title, Add card, and List menu controls, reporting
        the selected list and action to the board feature without mutating data.
      - [x] Port card body Open card and Card menu controls with exact card action
        reporting, plus persistent open/close card-details feature state.
      - [x] Render a feature-owned card-details canvas with exact-card Edit title,
        Archive, and Close intents, idle reset, and stale-selection handling.
      - [x] Port editable title input and persist card mutations through adapters.
        A bounded Nuklear field loads the authoritative scoped title/version,
        accepts 1–128 UTF-8 bytes, detects a 129-byte overflow, and offers Save,
        Cancel and Close. SQLite callbacks enforce actor/board/card scope,
        optimistic versions and durable request identities; models change only
        after commit. Fake UI, actual Nuklear input and integrated SQLite tests
        cover success/cancel, empty/oversized/control/invalid UTF-8 input, stale
        versions, wrong scope, replay, injected rollback and reopen persistence.
      - [x] Connect the explicit native Archive card action through the same
        adapter, using an authoritative version snapshot and closing details only
        after commit. Test stale/scope/replay rejection, rollback, cache visibility
        and persistence after reopening the database.
      - [x] Add a bounded, all-or-nothing SQLite board snapshot adapter with
        deterministic hierarchy order, strict row/type/UTF-8/parent validation,
        archived-card flags, and explicit capacity failure. A concurrent WAL writer
        regression verifies the reader never combines different snapshots.
      - [x] Compose the current modules in an optional POSIX desktop executable:
        verified embedded migration/catalog, existing local actor/board preflight,
        SQLite startup checks, SDL input/resize/quit loop, details adapters and
        session-local collapse state. Headless real-executable tests cover startup,
        invalid arguments/scope/files and unchanged logical data in smoke mode.
      - [x] Implement scoped Add card editing/persistence and cache insertion.
        Preserve board/list/rendered-swimlane intent; validate explicit parent IDs
        rather than defaulting native creation. Bound input and cache capacity,
        append only after commit, namespace generated IDs by actor/route/request,
        and test real input, cancel, malformed scope, replay, rollback and reopen.
      - [x] Add an explicit no-overwrite workspace initializer using private
        adjacent staging, the pinned migration, a seed transaction and atomic
        publication. Cover invalid titles/IDs/migration, existing destinations,
        concurrent creators and injected late failure. Editable seed titles share
        the 128-byte adapter limit; actor display names retain their 256-byte limit.
      - [x] Connect card movement to bounded native list/swimlane selectors and
        the existing optimistic SQLite operation. Reject stale/wrong/replayed
        requests; publish exact committed position and reorder the visible cache
        immediately. Cover same-column moves, SQL rollback, real UI and reopen.
      - [x] Add guarded native board/list/swimlane title editors and list/swimlane
        creation through the existing typed SQLite transaction adapter. New typed
        create operations do not expose unaudited HTTP routes. Test input, scope,
        version, replay, capacity, concurrent append order, rollback and reopen.
      - [x] Restore archived cards from a scoped native Archives panel through
        the shared guarded transaction boundary. Preserve card position, validate
        the archived version and update cache visibility only after commit. Cover
        empty archives, selection/cancel, scope/conflict/replay/rollback and reopen.
      - [x] Render the sidebar as an optional bounded overlay, keep explicit
        board actions focused, and open card actions through the details canvas.
        Use full-width wrapped card/list titles and separate action rows. Real
        Nuklear tests verify text/scissor visibility at the desktop's 14px font.
      - [_] Persist remaining list-menu and sidebar actions through adapters.
      - [x] Add explicit native list/swimlane reordering through the existing
        transaction adapter. Check the complete sibling-order fingerprint inside
        the transaction as well as selected-row scope/version, and publish the
        ordered cache only after commit. Cover first/last, no-op, stale sibling
        order, forged scope, replay, rollback, actual Nuklear input and reopen.
      - [x] Add indexed native card movement including archived rows and columns
        with position gaps. Preserve append compatibility; use a complete ordered
        snapshot fingerprint and atomic collision-safe compaction only for actual
        moves. No-op preserves positions and metadata. UI, SQLite and real SDL
        regressions cover target ordinals, concurrent order changes and reopen.
      - [x] Share strict model identifier/title/UTF-8 validation across native and
        SQLite adapters. Add focused Escape cancellation and single-line Enter
        submission without duplicate held-key commits; multiline Enter stays text.
      - [x] Add exact additive schema-v2 card descriptions through one compiled
        migration registry. Preserve v1 bytes, verify all contiguous typed history,
        upgrade old restore copies before stopping the listener, and test rollback,
        process interruption, concurrent upgraders, downgrade and malformed schema.
      - [x] Complete desktop card-description integration. Bounded multiline UI
        and SQLite suites cover empty/clear, 1024-byte limit, UTF-8, controls,
        cancellation, stale shared-card versions, scope, replay, rollback and reopen.
        Actual SDL events save multiline Finnish/Greek text; reopening and Cancel
        preserve exact database text and versions.
      - [x] Add pure C89 checklist/item models, exact parent validation, bounded
        UTF-8 titles, deterministic ordering and derived WeKan completion/visibility
        semantics. Preserve minicard inherit/false/true and empty-list behavior.
      - [x] Persist and edit checklist collections through additive schema-v3,
        existing guarded transactions and a complete bounded native panel. Support
        checklist create/rename, item add/rename/completion, and hide-checked/hide-all.
        Validate card and changed-row versions; no-op writes no metadata. Reload
        after commit; failed reload disables edits and retries only the read.
        Fake/real Nuklear, SQLite and actual SDL tests cover bounds, counts,
        visibility, scope, stale/replay/no-op, rollback, concurrent changes and reopen.
      - [x] Add a bounded session-local literal card-title filter that preserves
        the complete snapshot and parent mutation scope. Apply/Clear/Enter/Escape
        handle drafts and close hidden selections; exact UTF-8 plus ASCII case
        folding is an explicit subset of upstream regex search, not full parity.
      - [x] Add confirmed permanent checklist/item deletion through the same
        scoped transaction. Show exact stored targets and every affected child;
        Cancel/Escape write nothing and Enter never confirms. Test stale scope,
        replay, late rollback, ignored-child survivor rejection, preserved position
        gaps, reopen and post-commit refresh failure without a second deletion.
      - [x] Add atomic checklist batch entry in the existing native item editor.
        The canonical newline checkbox switches to multiline text; one guarded
        request appends up to eight trimmed, nonempty 128-byte titles in order.
        Keep the existing 4097-byte request limit, preserve all-or-nothing writes,
        advance card/checklist versions once, and test capacity, scope, stale
        revisions, replay, late rollback, reopen and actual Nuklear input.
      - [x] Add same-card checklist and item reordering through a bounded native
        Move selector and guarded SQLite transaction. Validate the whole card
        collection before collision-safe position compaction, advance only moved
        rows plus the appropriate parent/card revisions, and cover stale sibling,
        scope, replay, rollback, max-position and real Nuklear cancellation.
      - [x] Add compact native checklist count badges behind a persisted board
        opt-in. Schema-v6 keeps the original board table immutable using a
        default-false extension row; a complete bounded summary is loaded only
        after writes or explicit refresh, never during drawing. Empty lists show
        0/0 and their badge opens the exact card checklist panel.
      - [x] Move whole checklists between active cards on the same board through
        a native destination selector and guarded SQLite transaction. Preserve
        children/flags/positions, check both card revisions, and cover cancel,
        scope, capacity, stale/replay/rollback, failed-refresh recovery and reopen.
      - [x] Move individual items between checklists on the same board, including
        cross-card and same-card destinations. Guard all affected parents and the
        item, preserve completion, append atomically and verify rollback/reopen.
      - [x] Render cached expanded native checklist previews and expose explicit
        inherit/hide/show overrides. Respect hidden/completed-item flags, keep
        count visibility independent and open the exact card editor on title click.
      - [x] Persist the default-on expanded checklist board preference with an
        additive schema-v7 extension. Save both display preferences atomically
        through shared boolean storage and one settings panel; preserve overrides.
      - [x] Toggle inline checklist completion using shared guarded mutations,
        captured exact IDs/revisions, one-shot intents and read-only error recovery.
      - [x] Share persisted per-user checklist collapse between minicards and
        opened cards through reusable card-section preferences and controls.
        Validate actor/card scope, optimistic versions, capacity, rollback, refresh
        and reopen; keep canonical card/board revisions unchanged.
      - [x] Share a text form for expanded minicard checklist/item rename and
        single-item creation, with guarded one-shot writes and retained errors.
      - [x] Reuse single/batch checklist entry across opened cards and minicards,
        preserving drafts, canonical parsing and atomic guarded saves.
      - [x] Reuse a bounded drag-to-reorder control for minicard checklist titles
        and items within their sibling collection. Capture exact IDs/revisions;
        validate hidden sibling ordinals, rollback and consumed intents.
      - [x] Share explicit destination drop zones for whole-checklist transfer to
        another card and item transfer to another checklist on the same board.
        Append through existing guarded transactions with target revisions.
      - [x] Reuse a paginated board/card destination chooser for explicit cross-board
        checklist/item moves, with scoped snapshots, stale selection rejection,
        cancellation, atomic rollback and SQL-free directory rendering.
      - [x] Add explicit insertion ordinals to checklist/item transfer forms using
        one bounded position control and the existing collision-safe order writer.
        Preserve default append behavior, advance moved rows only once and guard
        changing sibling versions, terminal positions, rollback and exact scope.
        Fifteen backend combinations and real UI controls pass, including sanitizers.
      - [x] Reuse exact insertion mutations for minicard checklist/item drop slots
        before visible siblings. Include hidden sibling ordinals, capture target
        revisions, keep drawing SQL-free and consume each drop once.
      - [_] Finish cross-board drag movement and visual parity.
      - [x] Add board-scoped schema-v5 labels and card assignments through the
        existing transaction boundary. Preserve v1-v4 bytes and exact canonical
        empty-name/default-color/hex semantics; validate full bounded catalogs.
        Native board/card panels support create/edit/assign/unassign/confirmed
        delete, including removal from archived cards. Scope, stale versions,
        duplicate/no-op, replay, late rollback, reopen and real SDL input passed.
      - [x] Complete cached label badges in the board view. Full 2048-card,
        128-label bitsets and pure scoped rendering pass focused tests; desktop
        card-label workflows, restart, cancellation and package integration pass.
        Clicking an assigned badge opens the same exact-card labels panel.
      - [x] Centralize item/board color contracts and strict ECMAScript UTF-8 trim
        in model modules. Reuse existing parser semantics and select black/white
        label text from sRGB luminance without a new runtime dependency; canonical
        parity and independent contrast/UTF-8 regression suites pass.
      - [x] Persist scoped collapse preferences with validated atomic files and
        explicit failed-save recovery. Test input/path/scope/capacity failures,
        write rollback, pruning, actual SDL restart/actor isolation and smoke
        immutability; do not retry a failed write every unchanged input frame.
      - [x] Add a measured card-scoped checklist index without rewriting history.
        The unchanged query uses 114 instead of 48,121 SQLite VM steps in the
        documented fixture and stays constant after doubling unrelated items.
        Verify old-bundle upgrades, restored backups and altered-index rejection.
  - [_] Add server adapters for SQLite, REST, files, migrations, and import/export.
- [_] Convert Meteor 3 schema to SQLite schema that is optimized for fast queries
  - [_] Replace WeKan+FerretDB directly by using its existing SQLite directory and
    document layout without export/import. Pin the upstream layout, detect versions,
    lock out concurrent owners, back up before writes, migrate atomically with rollback,
    and reject corruption or unknown formats before any mutation.
    - [x] Pin and document FerretDB v1 commit `9ab2ca69` plus the observed real WeKan
      layout: database-per-`<name>.sqlite`, `_ferretdb_collections` metadata, opaque
      physical STRICT tables, and one `_ferretdb_sjson` SJSON document column with
      `indexFormat:2` (not Wena's relational schema). Add a strict-C89 read-only probe
      for absolute paths, quick integrity, metadata JSON, required collection mappings,
      physical column shape and zero header versions. Unknown/corrupt/missing layouts
      fail closed; direct writes remain blocked on codec/locking/round-trip parity.
    - [x] Add a shared bounded C89 JSON reader for SJSON/import adapters. Preserve
      exact numeric lexemes and object order, reuse UTF-8 decoding, reject duplicate
      decoded keys and malformed scalar Unicode, and publish snapshots atomically.
      Test byte/depth/node limits and a byte-mutation corpus under ASan/UBSan.
    - [x] Validate canonical pinned SJSON schemas and all 13 BSON types in one
      reusable read-only snapshot. Preserve exact integer/date/timestamp numbers,
      nested field order, binary subtype and regex options; reject range/type/schema
      mismatches before publication. Verify C89 and sanitizer byte-mutation tests.
    - [_] Implement typed codec writes, owner locking,
      verified backup and copied-WeKan round trips before direct replacement.
- [_] Using Nuklear GUI components, create same UI layout
  - [x] Establish MIT-licensed SVG source artwork/theme tokens and conversion to
    compact C89 native vector commands. Integrate a scale-aware board pictogram,
    preserve live text/canonical colors, reject unsupported SVG, and test real
    Nuklear 1x/2x/4x geometry plus exact documentation-image conversion.
  - [_] Extend SVG primitive coverage as component artwork is ported; keep every
    first-party asset vector-authored and generate platform representations without
    embedding multiple raster sizes. No GPL SVG rendering dependency.
  - [_] Theme the Native Nuklear GUI and Legacy HTML4 from one semantic token catalog
    to match current Meteor 3 WeKan for identical data, route, viewport and state.
    Capture ground-truth screenshots for every current theme and meaningful responsive,
    focus, hover, disabled, error and RTL state; derive colors, fonts, dimensions,
    spacing, borders, state colors and icons instead of using screenshots as artwork.
    Preserve HTML4/IBrowse/NetSurf/Dillo structure and ASCII fallbacks, and compare
    deterministic goldens with tolerance only for documented font rasterization.
    - [x] Pin the first shared-token gate to WeKan revision `689a3938`: require Wena's
      common C UI contract to contain exactly all 25 board themes and 25 item colors
      from Meteor's Legacy HTML4 source with byte-identical RGB values. Native and
      HTML4 already consume the common contract; non-color tokens, reference captures,
      viewport/state goldens, contrast and focus parity remain pending.
    - [x] Apply a readable native light default using canonical shared colors:
      light panels, white controls, dark text and blue accents. Test normal,
      hover, active, header and selection contrast and actual Nuklear commands.
      This is one native default; complete component/theme screenshot parity,
      typography, focus behavior and responsive/RTL states remain open.
- [_] Import/Export from WeKan, Trello, etc via WeKan REST API, Trello API, etc
- [_] Match current Meteor 3 WeKan environment variables, REST API contracts, and
  password/LDAP/OAuth2/OIDC/CAS/SAML login behavior without copying JavaScript or
  exposing credentials. Derive compatibility from source rather than a manual list;
  preserve defaults/coercion, proxy URLs, validation, status bodies, auth, timeout,
  retry/rate-limit and restart behavior, then implement one provider/API group at a time.
  - [x] Add a deterministic machine-readable inventory generator pinned to the current
    WeKan revision. Scan executable JavaScript plus start/build/deployment files for
    environment names and provenance, literal WebApp HTTP method/path registrations,
    and authentication-provider implementation sources. A regeneration parity test
    rejects stale inventory and requires core ROOT_URL, LDAP, OAuth2, CAS and login/
    logout surfaces. Semantic defaults and runtime implementations remain pending.
- [_] Nuclear GUI adapts to all screen sizes from smallest to biggest, with mobile and desktop mode, like Meteor 3 WeKan
- [_] GUI works with touch displays, mouse, keyboard
- [_] Possible to drag drop same way like Meteor 3 WeKan
  - [x] Reuse drag gestures and ordered-card snapshots for same-column card moves.
    Include archived/filtered sibling ordinals, exact IDs and source revisions;
    keep rendering SQL-free and apply the existing order fingerprint transaction.
    Explicit failed-write Refresh reloads the complete board and registered cache
    count atomically. Validate duplicate titles, rollback, external sibling changes,
    consumed intents and unchanged models on failed reload.
  - [x] Reuse explicit destination zones for card append moves to another column
    or swimlane, including empty destinations. Preserve source order/revision
    checks and exact destination scope, reject unavailable targets, and test late
    rollback plus cache publication with actual Nuklear/SQLite.
  - [x] Reuse the bounded drag controller and Move dialog's ordering checks for
    list/swimlane reordering. Read once after press, render without SQL and apply
    one guarded transaction after release. Preserve explicit error/Refresh,
    cancellation, collapsed-lane controls and repeated board-wide list identities.
    Real Nuklear/SQLite tests cover both kinds, revisions, sibling fingerprints,
    late rollback, cache publication and no replay; focused suites/sanitizers pass.
  - [_] Complete arbitrary insertion-point movement and visual parity.
- [_] Collapse Swimlane, List, Card etc like Meteor 3 WeKan
  - [x] Add bounded, board-scoped swimlane/list collapse state with canonical
    Collapse/Uncollapse controls, stable object IDs, nested restoration, stale and
    archived-object pruning, capacity handling, and board-switch isolation. Shared
    board-wide lists render in each active swimlane with distinct widget IDs and
    exact card-parent filtering; explicitly scoped lists retain their behavior.
  - [x] Persist whole-minicard folds per actor using the shared section control
    and store; keep title/actions visible and skip badges/expanded contents.
    Verify real mouse collapse/expand, read-only behavior, actor isolation and reopen.
  - [x] Add the canonical default-on board option to disable minicard collapse,
    preserving actor choices and sharing atomic board display-setting saves.
    Schema v9 upgrades from all earlier versions; rollback and real UI tests pass.
  - [_] Complete responsive/accessibility/persistence parity.
