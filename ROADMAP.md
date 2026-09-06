# WeKan - WeKan Native

- Made with C89, SDL2, Nuklear GUI, SQLite.
- One executeable GUI binary, that saves files to wekan-files directory structure like Meteor 3 WeKan FerretDB SQLite
- All code compatible with MIT license. No GPL code.
- Newest dependencies, that does not have vulnerabilities.
- Drag drop, looks same like Meteor 3 WeKan.
- For all desktop and mobile operating systems.
- Based on Meteor 3 WeKan https://github.com/wekan/wekan/models
- Local mode: Uses local SQLite database for read and write
- Remote mode: Uses WeKan REST API with any Meteor 3 WeKan URL for read and write
- Import Export between local and remote
- Uses WeKan Jade UI layout, with Nuclear UI components

# Roadmap

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
- [_] Add fast native equivalents of WeKan test categories, running against the
  current OS/CPU executable wherever behavior crosses a process boundary:
  - [x] Strict-C89 model/unit and negative-validation suites.
  - [_] Nuklear component, interaction-state, accessibility, keyboard, mouse,
    touch, responsive-layout, drag/drop, and collapse suites.
  - [_] Headless SDL executable smoke/startup/crash and command-line suites.
  - [_] SQLite schema, migration, transaction, corruption, concurrency, and query
    performance suites using temporary databases.
  - [_] REST contract/authentication/authorization/rate-limit and negative suites.
  - [_] WeKan/Trello import-export round trips, malformed input, attachment paths,
    and local/remote boundary suites.
  - [_] Platform artifact-format, dependency/license, sanitizer, fuzz, leak, and
    performance regression suites; run independent native suites in parallel.
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
      migration runner and production adapter remain unimplemented.
    - [x] Add the smallest strict-C89 system-SQLite migration runner with a bundled
      MIT-compatible SHA-256 verifier: reject modified SQL before open, apply version 1 with
      `BEGIN IMMEDIATE`, checksum metadata and `user_version` in one transaction,
      refuse gaps/downgrades, configure busy timeout/WAL/full sync/foreign keys, and
      gate startup on quick/FK checks with a full integrity API. Temp-db tests cover
      idempotent reopen, uncommitted crash rollback, bad SQL rollback, modified hash,
      newer schema, corruption, and the SHA-256 known vector. Migration embedding,
      backup execution, and the production persistence adapter remain pending.
    - [x] Pin migration bytes, size, schema version, and SHA-256 in a reviewed lock;
      fail every local/CI target before compile when it is stale. Append the exact
      migration plus a checksummed length footer to every ready single-file artifact
      before the final i18n payload, and extract/compare it from a host executable.
      Runtime lookup from the executable and the SQLite transaction adapter remain.
    - [x] Add a production SQLite create/edit/archive callback with `BEGIN IMMEDIATE`,
      actor/board/FK authorization, exact idempotency tuple, optimistic card versions,
      complete region validation, and mutation+idempotency commit in one transaction.
      Temp-db tests cover success, replay, conflict rollback and restart persistence;
      listener registration and broader board/list/swimlane mutations remain pending.
      Idempotency metadata stores the actual SHA-256 of the fully encoded region
      response, and commit requires exactly one pending-to-checksummed transition.
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
      remains blocked on a separately versioned schema and migration design.
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
      Server persistence remains closed, so this is client wiring only.
    - [x] Render stable-ID move controls as a visible, ordinary HTML4 POST form with
      session, scoped one-use CSRF, and operation fields plus an initially hidden
      drag control bound back to that exact form. A same-origin external stylesheet
      hides enhancement controls without JavaScript; capability success alone swaps
      visibility. Cover the real listener's cookieless/no-JS GET and asset paths.
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
      AbortController timeout/failure restoration. No handler invokes it yet, so
      mutation dispatch and drag/drop remain closed.
    - [x] Discover every currently visible allowlisted region from renderer-owned
      stable IDs and versions instead of hard-coding the board. The integration suite
      combines no-JS HTML/socket fallback, capability-supported/unsupported and POST
      failure contracts, and strict-C behavioral malformed/oversized/stale/replay plus
      atomic multi-region/partial-update tests. A real browser-runtime E2E remains
      pending because the build environment provides no browser or JavaScript engine.

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
      - [_] Port editable title input and persist card mutations through adapters.
  - [_] Add server adapters for SQLite, REST, files, migrations, and import/export.
- [_] Convert Meteor 3 schema to SQLite schema that is optimized for fast queries
- [_] Using Nuclear GUI components, create same UI layout
- [_] Import/Export from WeKan, Trello, etc via WeKan REST API, Trello API, etc
- [_] Nuclear GUI adapts to all screen sizes from smallest to biggest, with mobile and desktop mode, like Meteor 3 WeKan
- [_] GUI works with touch displays, mouse, keyboard
- [_] Possible to drag drop same way like Meteor 3 WeKan
- [_] Collapse Swimlane, List, Card etc like Meteor 3 WeKan
