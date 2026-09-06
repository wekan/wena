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
  - [_] Configuration model/UI: disabled by default; explicit IPv4 bind address and
    validated port (for example `127.0.0.1:3000`), with restart/status/error state.
  - [_] Server adapter with bounded HTTP parsing, connection/request limits,
    timeouts, authentication tokens, authorization, audit logging, and safe CORS.
  - [_] Versioned WeKan-compatible REST routes for users, boards, swimlanes, lists,
    cards, checklists, comments, labels, members, attachments, and activities.
  - [_] Back routes with the same SQLite model/storage layer used by local mode and
    preserve transactional parent relationships.
  - [_] Let remote clients select either a Meteor 3 WeKan base URL or Wena Server
    base URL, with capability/version discovery and compatible error handling.
  - [_] Import/export and local-to-remote/remote-to-local round-trip tests against
    both server implementations, including auth failures and interrupted transfers.
  - [_] At configured `ROOT_URL` (scheme, host, port, and optional base path), render
    the same semantic accessible Legacy HTML4 Kanban pages and POST-button actions
    on the same WeKan URL families; generate every link/form action below ROOT_URL
    while listening only on the Admin-configured IPv4 address and port.
  - [x] Share one page/component contract with Meteor HTML4: canonical WeKan i18n
    keys/values, colors, one content-table baseline, ASCII controls, natural tab
    order, and REST-domain operations; do not create another visual/text catalog.
  - [_] Protect HTML4 sessions and mutations with scoped CSRF tokens, replay
    prevention, authorization, safe redirects, output escaping, and GET immutability.
  - [_] Add Meteor HTML4 route-parity and golden/contract tests, cookieless/no-JS
    HTTP E2E, forged-scope/CSRF/replay/open-redirect/escaping negative tests, and
    configured startup/restart/listener tests.

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
