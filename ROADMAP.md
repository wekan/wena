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
