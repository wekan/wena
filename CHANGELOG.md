# Upcoming Wena release

<details>
<summary>The release builds again: v0.03 stopped on every platform on a stale compiled-in license file</summary>

- wena4 log: every job of the v0.03 release stopped before compiling with
  `client/platform/notices_data.h is stale`. The licenses compiled into the
  executable include `config/release-dependencies.json`, which the AROS
  rename (aros-x86 to aros-amd64) changed without regenerating them. The
  current header matches its sources again.
- New suite `generated-sources` runs the checks every desktop build runs
  first (`scripts/check_desktop_sources.sh`: pinned dependencies, SVGs,
  migrations, translations, font and licenses), so a stale generated file
  fails the test run instead of the release. Its negative case changes the
  dependencies on a copy and requires the check to report the stale header.

Thanks to xet7.

</details>

<details>
<summary>Card details, Card Actions, the Add Card composer and the sidebar look and work like WeKan's</summary>

- Card details are WeKan's panel instead of a column of buttons: a header
  with the caret that collapses it, the title (click it to edit, as in
  WeKan), Card Actions, Maximize and Close Card, then the Labels,
  Description and Checklists sections, each with WeKan's caret, icon and
  16px gray heading and each foldable. They show the card's label chips, its
  description and its checklists, whose items can be ticked there.
- Card Actions (the hamburger on a minicard and in the details) is WeKan's
  popup with the items Wena carries out, in WeKan's order and groups: Move to
  Top, Move to Bottom, Move Card and Move Card to Archive. Move to Top and
  Bottom are new: one version-checked reorder within the card's list. As in
  WeKan, the minicard's hamburger no longer opens the details.
- Add Card is WeKan's inline composer in the list: a white card with the
  text box, the blue Add and the close cross, above the cards for Add Card
  to Top of List and in place of "+ Add Card" for the bottom. Enter adds and
  the composer stays for the next card; a card added to the top is moved
  there, and a move that fails is reported.
- The sidebar is WeKan's home view, 420px under the header: the close
  cross, Board Settings, then foldable Members (the board's members and its
  own user), Labels (WeKan's colored chips) and Activities, and the Archive.
- Label chips on minicards and in the details are WeKan's: bold text on the
  label's color, 4px rounded, side by side. Checklists show their title in
  bold with a finished/total count, and items as WeKan's checkboxes.
- `--show card:ID`, `card-menu:ID`, `list-menu:ID`, `add-card:LIST` or
  `sidebar` with `--screenshot` renders each state WeKan's capture has, for
  comparison.
- Fixed on the way: Nuklear's `nk_spacing` on a one-column row starts a
  new row, which left an empty row after each read-only checklist item;
  raw colors in the look module were drawn black.
- Tests: `card-actions` (suite) drives the popup's items against SQLite,
  with disabled items and unknown cards as negatives; move to top and bottom
  are in `card-move-reorder-sqlite`; the composer, details header and
  sections, sidebar folds and chips are in `card-create`, `board-feature`,
  `card-description`, `nuklear-checklist-contents` and `nuklear-board`.

Thanks to xet7.

</details>

# v0.03 2026-10-04 Wena release

<details>
<summary>The NetBSD, DragonFly BSD, Haiku and OpenBSD builds compile again; FreeBSD riscv64 waits for packages</summary>

- wena3 log: NetBSD amd64 and arm64, DragonFly BSD and Haiku stopped on
  `server/executable_path.c`: their GCC rejects an `if` with more statements
  after it on a line of its own (`-Werror=misleading-indentation`), which
  clang - and so the header check of v0.02 - accepts. Those lines now hold one
  statement each. `tests/test_bsd_sources.py` also compiles every desktop
  source with GCC 13 on NetBSD's headers (the host's GCC, or the pinned
  `gcc:13` image), reproduced the error at the same line before the fix, and
  rejects a probe of the pattern.
- OpenBSD amd64 and arm64 stopped unpacking SDL2: OpenBSD's `tar` has no
  `--strip-components`. `scripts/extract_archive.py` unpacks the pinned
  archives without their top directory on every system, keeping modes and
  links and refusing entries or links that leave the destination
  (`tests/test_extract_archive.py`, suite `extract-archive`).
- FreeBSD riscv64 is planned again: FreeBSD publishes no riscv64 packages for
  14 or 15, so its virtual machine has no Python, make or X11 to build with.
  Cross-compiling it from Linux is the way there.
- Verified here: GCC 13 on NetBSD 10.1's headers reproduced the release
  run's error at `executable_path.c:31` and passes after the fix; all desktop
  sources compile with clang for FreeBSD, OpenBSD and NetBSD and with GCC for
  NetBSD; the pinned SDL2 and llvm-mingw archives unpack and run. The BSD and
  Haiku builds themselves are verified by the next release run.

Thanks to xet7.

</details>

<details>
<summary>The AROS x86-64 release file is wena-aros-amd64, named for its CPU as every other file is</summary>

- `wena-aros-x86` was an x86-64 file (`ELF 64-bit LSB relocatable, x86-64`),
  while "x86" names 32-bit x86 everywhere else. The target is now
  `aros-amd64`, its file `wena-aros-amd64`, as `wena-linux-amd64` and
  `wena-windows-amd64.exe` are.
- The release check takes the CPU from the name: an `aros-amd64` file must
  be a 64-bit x86-64 relocatable ELF and an `aros-i386` one a 32-bit i386
  one, and an AROS name with any other CPU is refused. The target catalog
  allows no target ending in the ambiguous `-x86`.
- `config/targets.tsv` lists AROS per CPU: `aros-i386` for the 32-bit ABIv0
  line of deadwood2/AROS (planned until its build is in the release
  workflow), AROS on m68k running `wena-amigaos-m68k`, and AROS on ARM having
  no published toolchain or SDK to build with.

Thanks to xet7.

</details>

<details>
<summary>The desktop opens again after it was closed normally</summary>

- Run, and a double-click on the desktop, said "Unable to open the local Wena
  desktop" for a board that was there. The startup check that an actor or
  board named by mistake writes nothing opened the file read-only, and a
  read-only handle cannot read a WAL database whose `-wal` file a clean exit
  removed ("unable to open database file"). So every launch after a normal
  quit failed, and only a launch after a crash worked.
- The check now opens the existing file read-write without creating it, with
  `PRAGMA query_only=ON`: a missing file still fails, and nothing can be
  written. The debug log names which check failed and SQLite's reason.
- `tests/test_desktop.sh` closes a WAL workspace cleanly (no `-wal` or `-shm`
  file) and opens it, and checks that an unknown actor there still changes
  nothing. With the read-only handle back, it fails as Run did.

Thanks to xet7.

</details>

<details>
<summary>The desktop board looks and works like WeKan's: its colors, fonts, header, lists, cards and menus</summary>

- Measured from WeKan, not chosen by eye: `tools/wekan-ui/capture.e2e.js`
  runs in WeKan's Playwright suite against a running WeKan and records each
  state of a seeded board (the board, list and swimlane menus, Add Card, the
  sidebar, card details and its menu) with every visible control's text,
  tooltip, icon, place and colors. `tools/wekan-ui/pin.py` pins them in
  `tests/fixtures/wekan-ui`.
- `client/components/common/wekan_look.c` holds WeKan's colors (the
  `#2980b9` header, `#dedede` canvas, `#e4e4e4` list headers, white
  minicards with `#4d4d4d` text, white popups with gray title bars, `#f7f7f7`
  panels, the red `#ce1414` of a list over its WIP limit), its fonts (Roboto
  and Roboto Bold, now embedded with its provenance, at WeKan's 12 to 19 px)
  and its Font Awesome icons as vectors. The Nuklear theme uses the same
  palette.
- The same controls in the same places, named as WeKan names them: the header
  with the board title, Filter, the user and the sidebar toggle; swimlane
  headers with their caret and Swimlane Actions; list headers with Collapse,
  Add Card to Top of List, Add List and List Actions, and a long title that
  wraps; minicards with their caret and Card Actions; "+ Add Card" under each
  list; collapsed lists as a narrow strip with the title stacked; WeKan's
  popup menus for lists, swimlanes and the user, the Filter panel and Change
  Language.
- Dragging works as in WeKan: the whole minicard, list header or swimlane bar
  is the handle, and a press and release without moving is a click that opens
  the card or edits the title. The swimlane resize handle is WeKan's 10 px
  bar, shown when hovered.
- Every control is recorded with its WeKan name and place each frame, which
  is what tests click. `--screenshot FILE` saves the last frame as a BMP, to
  set beside WeKan's captured screenshots.
  An icon's tooltip is drawn at window level: opened from inside a header
  row, it broke the frame, so nothing after the hovered icon was drawn.
- `tests/test_wekan_ui_parity.py` (suite `wekan-ui-parity`) checks every
  Wena color against WeKan's capture, and the WIP color against WeKan's
  stylesheet when the WeKan checkout is next to Wena. The board, list, card,
  filter, theme, SVG, collapse and swimlane-resize suites now click WeKan's
  control names and check WeKan's colors, including the hovered tooltip, the
  sidebar opening below the header, and a missing selection marking nothing.

Thanks to xet7.

</details>

# v0.02 2026-10-03 Wena release

<details>
<summary>The desktop for AmigaOS 3.x, AmigaOS 4 and AROS: wena-amigaos-m68k, wena-amigaos4-ppc, wena-aros-x86</summary>

- `scripts/build_desktop_amiga.sh TARGET OUTPUT` compiles the desktop in the
  pinned `amigadev/crosstools` image of each into one static executable: the
  image's SDL2 2.30 on AmigaOS 4 (PowerPC ELF); SDL2 2.32.10 with AROS's own
  port (`SDL2-2.32.10-aros.diff` from aros-development-team/contrib, without
  OpenGL) on AROS x86-64 (relocatable ELF); and on AmigaOS 3.x the SDL2 fork
  DevilutionX ships for 68040 with FPU and an RTG card (HUNK). Images and
  sources are pinned by digest and SHA-256.
- SQLite runs there without WAL, mmap or file locks, through an `amiga` VFS
  that keeps AmigaDOS names such as `PROGDIR:x` as they are. Wena's
  `journal_mode=WAL` then simply stays `delete`; other platforms keep WAL.
- The platform code knows `Volume:` paths, keeps the board in
  `PROGDIR:wena.sqlite` (`ENV:` is a RAM disk), publishes a new workspace with
  dos.library `Rename()` (which never replaces), retries settings writes
  because AmigaDOS `Rename()` does not replace, keeps the collapse preferences
  file within the original FFS's 30 characters, asks `locale.library` for the
  language and runs on a 1 MB stack (AmigaOS 4 `$STACK` cookie, libnix
  `__stack`, AROS `NewStackSwap`).
- Verified here: all three build, link statically and pass the format checks
  (`scripts/check_release_executable.py` now knows HUNK, static PowerPC ELF
  and AROS's relocatable ELF); the desktop with the exact Amiga SQLite options
  and VFS creates and reopens a board on Linux. Not run on an Amiga or an
  emulator yet. Tests: `amiga-desktop`, `debug-log`, `build-entrypoints`.

Thanks to xet7.

</details>

<details>
<summary>The desktop for Android and iOS: wena-android-arm64.apk and wena-ios-arm64.ipa</summary>

- `scripts/build_desktop_android.sh OUTPUT_APK` builds `libmain.so` - the
  desktop with SDL2 2.32.10 and SQLite linked in - with SDL's own Java glue and
  a small `fi.wekan.wena.WenaActivity`, directly with the NDK, javac, d8,
  aapt2, zipalign and apksigner (no Gradle). Min SDK 21, target SDK 37,
  16 KB-page aligned. It is signed with the release key from the repository
  secrets, or a debug key with a warning.
- `scripts/build_desktop_ios.sh OUTPUT_IPA` builds SDL2 for iOS with CMake and
  links `Payload/Wena.app` (iOS 15 or newer), unsigned: re-sign it with your
  own certificate, AltStore or Sideloadly. Apps built with the iOS 27 SDK must
  use scenes, which SDL 2 does not, so `client/platform/ios/scene.m` puts
  SDL's windows into the app's scene.
- On a phone the board lives in the app's own data folder
  (`SDL_GetPrefPath`), the language comes from the system, the board is laid
  out in density-independent units and drawn at native pixels, touches land
  where they are drawn, the keyboard shows only while a field is edited, and a
  failure is shown in a message box. On Android a new workspace is published
  with `rename()` after checking nothing is there, because SELinux refuses
  `link()` in the app's folder.
- Verified here: the APK passed the smoke test in the Android 17 (API 37)
  emulator, on a second run and after an update over itself, and a card was
  added by touch; the Simulator app passed on iOS 27.0 and 26.5. Real phones,
  Android 5-16 and Xcode 16 builds are verified by use and the release run.
  `scripts/check_release_executable.py` checks the APK's only library is
  arm64 `libmain.so` loading Android's own libraries, and the IPA's `Wena`
  loads only iOS's frameworks. Tests: `mobile-desktop`, `debug-log`,
  `build-entrypoints`.

Thanks to xet7.

</details>

<details>
<summary>Every release file is the desktop GUI, one self-contained wena-TARGET per platform, from one workflow</summary>

- `release-all.yml` is the only release workflow. `release-desktop.yml`, the
  `.github/release/*.sh` scripts and `client/main.c` are gone: the program they
  released printed one line in a terminal. Every target in `config/targets.tsv`
  is now the native Nuklear desktop, named after its platform -
  `wena-linux-amd64`, `wena-windows-amd64.exe`, `wena-freebsd-amd64` - and
  attached beside one `SHA256SUMS`, without a `.sha256` per file or a separate
  notices archive.
- 28 platforms: Linux amd64, arm64, armhf, armel, i686, riscv64, ppc64le,
  s390x and mips64le; FreeBSD amd64, arm64 and riscv64; NetBSD and OpenBSD
  amd64 and arm64; DragonFly BSD and Haiku amd64, built natively in virtual
  machines (cross-platform-actions v1.6.0, `scripts/build_desktop_release_vm.sh`);
  macOS arm64 and amd64; Windows amd64, i686 and arm64; and, below, AmigaOS
  3.x, AmigaOS 4, AROS, Android and iOS.
- SDL2 and SQLite are linked into each one from the pinned sources, as before;
  the licenses of everything in it are now compiled in too
  (`scripts/generate_notices.py`), and `wena --licenses` prints them.
- wena2 log: linux-armel and linux-mips64le stopped at once, because the
  `debian:bookworm` image no longer lists those CPUs. They build in Debian's
  per-architecture images, `arm32v5/debian:bookworm` and
  `mips64le/debian:bookworm`, with the same glibc 2.36. Under QEMU, loading
  Mesa's DRI driver crashes on MIPS64 (SIGBUS) before Wena draws anything,
  whichever Gallium driver is chosen, so that one X11 smoke test keeps Mesa's
  driver unloaded and uses SDL's software renderer. Every X11 smoke test now
  checks the application's own exit status rather than `xvfb-run`'s.
- Two compile errors that would have stopped the BSD builds, found by
  compiling every desktop source against FreeBSD 15.1, OpenBSD 7.9 and NetBSD
  10.1 headers (`tests/test_bsd_sources.py`, suite `bsd-sources`): an unused
  static helper in `server/executable_path.c` on FreeBSD, NetBSD and DragonFly
  (`-Werror`), and NetBSD's `<sys/sysctl.h>` needing `_NETBSD_SOURCE`.
- `./build.sh build TARGET` builds the same release file into `release/` the
  way the workflow does: Linux targets in the workflow's own container image
  (`scripts/toolchain.py` and the workflow are checked to agree), Windows with
  MinGW-w64 (arm64 with the pinned llvm-mingw, now also on macOS), macOS with
  Xcode. A BSD or Haiku target builds on that system itself.
- Verified here: macOS arm64 and amd64 built, ran headless twice and printed
  their licenses; Windows amd64, i686 and arm64 built and were checked
  self-contained; linux-arm64 and linux-armel built and passed the headless
  and X11 smoke tests in their containers, and linux-mips64le too, under QEMU. The BSD and Haiku virtual-machine builds, the Windows runs and
  the other Linux CPUs are verified by the next release run. Tests:
  `release-workflow`, `target-catalog`, `toolchain`, `build-entrypoints`,
  `source-structure`, `bsd-sources`, with negatives (the wena2 image, a target
  left out of the workflow, the two BSD errors, a stray release file).

Thanks to xet7.

</details>

<details>
<summary>The desktop compiles its migrations in and reads nothing from its own file</summary>

- It assembled the migration bundle from a footer appended to its executable,
  found through the executable's path, and opened an appended translation
  catalog only to check that it was there. An app bundle, an APK, a signed
  iOS app and Amiga's `PROGDIR:` either cannot carry appended bytes or cannot
  find the file reliably, and OpenBSD has no `/proc` to find it with. The
  bundle now comes from the compiled registry, checked against its own
  SHA-256 (`wena_sqlite_compiled_bundle`); nothing is appended.
- `tests/test_compiled_bundle.sh` (suite `compiled-bundle`) checks the bundle
  against `config/migrations-lock.json`, NULL arguments, and that neither the
  desktop nor its build reads or appends a footer. The runtime and
  embedded-migration suites test the server's footer reader with a fixture
  instead of the removed terminal program.

Thanks to xet7.

</details>

<details>
<summary>build.sh and build.bat install what a build needs on macOS, Windows, Ubuntu, Debian and Fedora</summary>

- `scripts/toolchain.py` runs before every build and installs what is
  missing with the computer's own package manager: Homebrew on macOS, apt on
  Debian and Ubuntu, dnf on Fedora, Chocolatey or else winget on Windows.
  `build.sh` and `build.bat` install Python 3 first when it is missing.
- Per target: gcc, Debian's cross-compilers with their C library, MinGW-w64,
  Docker (started when it is not running, with QEMU on arm64 Linux) for
  AmigaOS and AROS, and the Android NDK r29. The NDK is downloaded from Google
  and checked against the size and SHA-1 in Google's own repository manifest
  before it is unpacked into `.tools`. iOS uses an installed Xcode through
  `DEVELOPER_DIR` without changing which one is selected.
- Where this computer has no compiler for a Linux target (Fedora, macOS,
  Windows), it is built in an Ubuntu 24.04 container with Ubuntu's compiler.
  The container's name is a hash of its package list, so a changed list
  builds a new one.
- On Windows, Git for Windows provides `sh` and `file`, a native MinGW-w64 gcc
  builds the Windows target, MSYS2 provides SDL2, SQLite and gcc for the
  desktop, and a `python3` shim lets the release scripts call Python.
  `android-arm64.sh` uses the NDK's prebuilt compiler for this computer
  instead of always Linux's.
- `build all` builds every target this computer can build and lists the
  others with the reason, such as iOS without Xcode. `install TARGET|all|desktop`
  installs without building. `WENA_NO_INSTALL=1` only checks.
- `windows-amd64.sh` accepts both ways file words a PE executable: file 5.46
  (Fedora 42) puts "Windows" before "x86-64".
- Verified on this Mac: all ten release targets and the desktop built after
  installing what was missing. On fresh Ubuntu 24.04, Debian 12 and Fedora 42
  containers, starting without Python, these all built: the host target,
  Windows amd64 and the desktop, plus armhf cross-built on Ubuntu and
  Debian. Windows was not run here. `tests/test_toolchain.py` covers each
  package manager and each target with fakes, including what is refused:
  an NDK download that fails its checksum, a zip entry outside its folder,
  a failed install and `WENA_NO_INSTALL`.

Thanks to xet7.

</details>

<details>
<summary>The macOS, iOS, AmigaOS, AROS and Android release builds compile again</summary>

- v0.01's `release-all.yml` built six of its ten targets' compilers into
  errors before a line of Wena was compiled.
- macOS arm64, macOS amd64 and iOS arm64: clang, found with `xcrun --find`
  and run by path, has no SDK of its own and found no `<stdio.h>`. It now
  comes from the `macosx` or `iphoneos` SDK and gets that SDK's
  `-isysroot`.
- AmigaOS 3.x m68k: libnix and `sys/types.h` declare `static inline`
  functions, and `inline` is not a C89 keyword; `-Dinline=__inline__` keeps
  `-std=c89 -pedantic-errors` for Wena's own code.
- AROS x86: the image's gcc has no include path. It gets
  `--sysroot=/opt/x86_64-aros` and the ISO C headers in `aros/stdc` ahead of
  the `aros/posixc` layer, which does not compile as C89.
- Android arm64: `sdkmanager` is not on PATH on the ubuntu-24.04 runner; the
  NDK step runs it from `$ANDROID_HOME/cmdline-tools/latest/bin`.
- Verified here: the failures reproduced, then macOS arm64 and amd64,
  AmigaOS and AROS built with `wena.py build` and passed their release
  checks. iOS (no iphoneos SDK here) and Android are verified by the next
  release run. `tests/test_release_workflow.py` pins each fix and fails
  against the old scripts.

Thanks to xet7.

</details>

# v0.01 2026-10-03 Wena release

<details>
<summary>Security: no test runs SQL read from outside the program (18 GitHub CodeQL cpp/sql-injection alerts)</summary>

- GitHub CodeQL code scanning reported alerts #2-#19, "Uncontrolled data in
  SQL query": tests read a schema file named on their command line and passed
  its text to `sqlite3_exec`. The same shape was in 62 tests. Their schema,
  fixture and migration SQL is now compiled in by
  `scripts/embed_test_files.py` and read through `tests/support/test_files.h`;
  a path argument only selects one of those files. Reads that only compare or
  hash database and backup files stay, each listed with its reason.
- The application was already clear: every value is bound with
  `sqlite3_bind_*`, migrations are embedded and checked against their pinned
  SHA-256, and the five places that format SQL text use fixed table and column
  names or SQLite's `%w` identifier quoting.
- `tests/test_sql_sources.py` (suite `sql-sources`) fails when a test that runs
  SQL reads a file at run time, when a script passes a `.sql` file without
  embedding it, or when the application builds SQL text anywhere new; the
  generator and lookup are tested, including an unknown name aborting the
  test. All converted suites pass on Linux; on macOS the same 11 suites fail
  as before, for Apple's SQLite.

Thanks to GitHub CodeQL.

</details>

<details>
<summary>One self-contained desktop executable per platform, numbered from Upcoming and released from the menu</summary>

- Menu option 3) Release (`build.sh release next`) numbers the Upcoming section
  after the newest release, the way WeKan does (v0.01 ... v9.99, v10.00),
  commits `Prepare vX release`, pushes, and starts `release-desktop.yml`.
  `build.sh release missing` builds and attaches to the newest release. It
  refuses uncommitted changes. Tests, Server and Tools move to 4, 5 and 6.
- `release-desktop.yml` publishes the release with that CHANGELOG section as
  its notes, starts `release-all.yml`, and builds 14 executables: Linux amd64
  and arm64 natively, armhf, armel, i686, riscv64, ppc64le, s390x and
  mips64le under QEMU; macOS arm64 and amd64; Windows amd64, i686 and arm64
  cross-compiled and smoke-tested on Windows runners. Each is attached with its
  `.sha256`, beside one notices archive of licenses and provenance.
- SDL2 2.32.10 and SQLite 3.53.4 are built from checksum-pinned sources and
  linked in (`scripts/build_desktop_release.sh`), SDL with only video,
  rendering and events. A Linux executable needs only glibc and X11 or
  Wayland, macOS only its own frameworks, Windows only its system DLLs;
  `scripts/check_release_executable.py` reads each executable's own library
  list and refuses anything else.
- The desktop runs on Windows: drive and UNC paths, wide-character file APIs,
  a workspace published with no-replace `MoveFileExW`, the board in
  `%APPDATA%\Wena`, and the debug log.
- gcc 13 rejected 27 one-line `if (...) a; b;` statements under
  `-Werror=misleading-indentation`; each is one statement per line now.
- Verified here: macOS arm64 and amd64 builds, Linux arm64 and amd64 built on
  Ubuntu 22.04 and smoke-tested under Xvfb (glibc 2.34), Windows amd64, i686
  and arm64 cross-built and checked. Tests: release flow with gh and git
  replaced, numbering and notes, the executable checker against ELF, PE and
  Mach-O with negatives, pinned downloads, packaging, and the workflow's
  platform list.

Thanks to xet7.

</details>

<details>
<summary>Drag the bar below a swimlane to change its height</summary>

- Each expanded swimlane has a bar below it. Dragging it up or down resizes
  the lane live, with the up-down resize cursor; release keeps the height,
  Escape cancels. Heights are clamped to 160-2000 pixels, the default 360
  needs no entry, and the lists inside grow with the lane.
- Heights are saved per actor and board with collapsed lanes and lists. The
  preferences file is version 2 exactly when it holds heights, so version 1
  files keep loading; heights of lanes the board no longer has are dropped.
- Tests: a real Nuklear drag test (press, drag, release, Escape, both clamps,
  back to the default, a collapsed lane and a layout without the bar), and
  preference round trips with strict negatives for every malformed height
  line, a full file, and pruning.

Thanks to xet7.

</details>

<details>
<summary>Fix the desktop crash when the mouse reaches a list or swimlane drag handle</summary>

- 42 sources included `<nuklear.h>` without the `NK_INCLUDE_*` options that
  `sdl_nuklear.h` set before compiling Nuklear itself. Those options add fields
  to `struct nk_context`, so the drag handles read `context->current` at another
  offset than Nuklear wrote it (0x4920 instead of 0x4a10), got NULL and crashed
  with SIGSEGV, as three macOS crash reports and the debug log showed.
- The options now live only in `client/platform/nuklear_options.h`, included
  before `<nuklear.h>` in every source and test, and the tests that compiled
  Nuklear with two or three of them now use the same set. The narrow clang
  C23-extension silence for Nuklear's alignof moved there too, so 24 Nuklear
  suites build and pass on macOS again.
- `tests/test_nuklear_options.py` (suite `nuklear-options`) fails when any of
  the 100 units includes Nuklear without the options first or sets an option
  elsewhere, with negative cases for both.
- With no workspace named, also when only `--smoke` or `--language` is given,
  the desktop opens the default board; the desktop suite tests its creation,
  reopening and a refused relative `WENA_DATABASE`.

Thanks to xet7.

</details>

<details>
<summary>Debug log for every desktop run, and the desktop opens its board when double-clicked</summary>

- Each run writes `.tools/log/wena/YYYY-MM-DD_HH-MM-SS/desktop.log`: the
  arguments, the board opened, the source line of any startup failure with
  SDL's error, the window closing, the exit status, and a fatal signal, written
  with async-signal-safe calls before the default action, so a crash is visible.
  `build.sh run` adds `run.log` with everything the app printed and whether it
  exited or was killed by a signal. `WENA_LOG_DIR` chooses another folder.
- Started without arguments, as a double-click does, `wena-desktop` opened
  nothing and printed that it needs a database, actor and board. It now opens
  the same local board as Run, created on the first run.
- Tests cover the log folder and default board rules with their negative
  cases, a written line and a recorded crash in a child process, and run.log's
  output, exit code and signal.

Thanks to xet7.

</details>

<details>
<summary>Run opens the native Nuklear desktop, and the desktop builds on macOS again</summary>

- Menu option 2) Run and `build.sh run` open `dist/desktop/wena-desktop` on a
  local board, created on the first run in the user's data folder or in
  `WENA_DATABASE`, instead of the bootstrap binary that only prints its name.
  Given arguments go to the desktop unchanged.
- The desktop did not compile with Apple clang 21: `SDL2/SDL.h` was not found
  through `sdl2-config`, `mkdtemp` was hidden by `_POSIX_C_SOURCE`, and the pinned
  Nuklear's alignof is reported as a C23 extension. The desktop now includes
  `SDL.h` like the other sources, defines `_DARWIN_C_SOURCE` on macOS, and
  silences only that warning around the two Nuklear headers on a clang that
  knows it. The `nuklear` and `sdl-text-input` suites pass on macOS again.
- Tests cover the desktop path per platform, the data folder per system and
  `WENA_DATABASE`, first-run creation and reopening, both refusals and passed
  arguments; the desktop smoke test passes for a created and a reopened board.

Thanks to xet7.

</details>

<details>
<summary>Add a Run option to the build menu</summary>

- `build.sh` and `build.bat` menu option 2) Run starts the binary that 1) Build
  wrote for the current computer, `dist/<target>/wena` or `wena.exe`. Tests,
  Server and Tools move to options 3, 4 and 5. The same is `run [ARGS...]` as a
  named command, which passes its arguments on.
- A missing or non-executable binary is refused with how to build it. Tests
  cover the file name per platform, both refusals, a real run with its
  arguments and exit code, and the menu order.

Thanks to xet7.

</details>

<details>
<summary>Preserve person assignments when moving cards between boards</summary>

- Reuse shared member filtering and strict SQLite readers in single-card and
  bulk cross-board transfers. Retain members active on the destination board,
  preserve assignees, and keep their original ordering positions.
- Verify card metadata, both board rosters, and person/actor data throughout the
  transfer transaction. Roll back ignored writes, partial transfers, and changes
  caused by later writes or triggers. Keep foreign-key enforcement enabled.
- Add regression coverage for inactive and absent destination members, empty and
  full 2048-person sets, roster overflow, deferred parent updates, rollback and
  reopening. Update the roadmap; native person capture adapters, roster
  management and person controls remain unfinished.

Thanks to xet7.

</details>
