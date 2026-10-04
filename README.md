# Wena — WeKan Native Kanban

Wena is an in-progress native WeKan implementation using C89, SDL2, Nuklear
and SQLite. It is not yet a complete WeKan replacement. [ROADMAP.md](ROADMAP.md)
records tested slices, remaining work and the next resume checkpoint.

## Local desktop development

`./build.sh` (`build.bat` on Windows) installs what a build needs on this
computer before running it: the compiler, SDL2 and SQLite development files and
the pinned MIT-licensed Nuklear source for the desktop, and for a release target
its cross-compiler, SDK, Android NDK or Docker. It uses Homebrew on macOS, apt on
Debian and Ubuntu, dnf on Fedora, and Chocolatey (or winget) on Windows; Python 3
is installed first if it is missing.

```sh
./build.sh build desktop
./dist/desktop/wena-desktop --database /absolute/path/wena.sqlite \
  --actor local-user --board my-board --create --title "My board" --language en
```

Use an existing parent directory. `--create` initializes a new local workspace
without overwriting files. Omit `--create` and `--title` to reopen it with the same
database, actor and board arguments.

The desktop supports creating cards, lists and swimlanes; editing their titles;
renaming the board; reordering lists and swimlanes; moving, archiving and restoring
cards; editing card descriptions and checklists with confirmed deletion and
whole-checklist transfers between active cards on the same board;
filtering card titles; and
remembering collapsed lists and swimlanes, and swimlane heights set by dragging
the bar below a lane, per local actor and board. Changes use guarded SQLite transactions and survive reopening.
The language selector changes canonical WeKan labels immediately and remembers
the selection. Local access trusts the operating-system user; the actor argument
is not a login mechanism.

The embedded Roboto font covers selected Latin, Greek and Cyrillic characters.
Complete WeKan feature parity, native drag/drop, full Unicode font/RTL coverage,
remote synchronization and GUI cross-platform packaging remain open. Direct
Meteor/FerretDB format migration is not implemented. See
[native desktop details](docs/native-desktop.md) for supported behavior and limits.

## SVG artwork and themes

UI artwork and native theme tokens use [MIT-licensed SVG sources](imports/ui/svg/README.md).
The desktop converts these at build time to compact C89 geometry and draws them
at the required scale through Nuklear. It does not bundle multiple raster sizes
or an SVG/XML runtime library. The initial native theme and board pictogram use
this path; complete theme/responsive parity remains in the roadmap.

## Tests and release builds

```sh
./build.sh tests all
./build.sh tests --list
./build.sh tests svg
./build.sh tests checklist-move
./build.sh tests nuklear-checklist-move
./build.sh tests card-editor-sqlite
./build.sh tests nuklear-editor
./build.sh tests sanitizers
./build.sh --list
./build.sh build host
./build.sh build all
./build.sh install all
./build.sh run
```

`build TARGET` builds one platform's release file into `release/`, the same
way the release workflow does: a Linux target in the workflow's own container
of that CPU (under QEMU for another CPU), Windows with MinGW-w64, macOS and iOS
with Xcode. `build all` builds every target this computer can build and lists
the others with the reason: macOS and iOS need a Mac (iOS needs Xcode, which
the App Store installs), and a BSD or Haiku target builds on that system
itself. `install TARGET|all|desktop` installs without building, and
`WENA_NO_INSTALL=1` only checks.

`./build.sh run` (`build.bat run` on Windows, or menu option 2) Run) opens the
native Nuklear desktop that `build desktop` (menu 1) Build, then d) Local
SDL2/SQLite desktop app) wrote, `dist/desktop/wena-desktop`, on a local board
where cards, lists and swimlanes can be dragged. The board is created on the
first run, as actor `local-user` and board `my-board`, in
`~/Library/Application Support/Wena/wena.sqlite` on macOS,
`$XDG_DATA_HOME/wena/wena.sqlite` (default `~/.local/share/wena/wena.sqlite`)
elsewhere, or the absolute path in `WENA_DATABASE`. Arguments after `run` are
passed to `wena-desktop` instead. It says how to build the app when it is
missing. Started without arguments, for example by double-clicking it,
`wena-desktop` opens the same board itself.

Each run writes a debug folder, `.tools/log/wena/YYYY-MM-DD_HH-MM-SS`, in the
`.tools` folder Wena is checked out in (or `WENA_LOG_DIR`): `desktop.log`
records the arguments, the board opened, the source line of any startup
failure with SDL's error, the window closing, the exit status and a fatal
signal such as a crash; `run.log`, written by `build.sh run`, keeps everything
the app printed and how it ended.

The test runner executes independent native suites in parallel, serializes
shared artifact builds, and reports failures and missing prerequisites
separately. Node.js enables the optional JavaScript runtime suite. Set
`WEKAN_ROOT` to the canonical WeKan checkout pinned by the parity tests when
running upstream source checks.

`build desktop` is the quick local build, against this computer's SDL2 and
SQLite. No release workflow is needed for local development.

## Releases

Every release is the desktop - the native Nuklear GUI - as one file per
platform, named after it: `wena-linux-amd64`, `wena-windows-amd64.exe`,
`wena-freebsd-amd64` and so on, listed with how each is built in
[config/targets.tsv](config/targets.tsv), beside one `SHA256SUMS`. Each file
has everything it needs linked in: SDL2 and SQLite from the checksum-pinned
sources in [config/release-dependencies.json](config/release-dependencies.json),
the font, the migrations and translations, and the licenses of all of it
(`wena --licenses`). A Linux or BSD executable needs only the system's C
library and X11 or Wayland desktop, a macOS one only macOS, a Windows one only
Windows. `scripts/check_release_executable.py` refuses a build when SDL2,
SQLite or a non-system library would be loaded at run time.

Platforms: Linux x86-64, ARM64, ARMv7, ARMv5, x86, RISC-V 64, POWER
little-endian, IBM Z and MIPS64 little-endian; FreeBSD x86-64 and ARM64;
NetBSD and OpenBSD x86-64 and ARM64; DragonFly BSD and Haiku
x86-64; macOS Apple silicon and Intel; Windows x86-64, x86 and ARM64; AmigaOS
3.x (68040 with FPU: `wena-amigaos-m68k` for an RTG graphics card,
`wena-amigaos-m68k-aga` for AGA), AmigaOS 4 and AROS x86-64,
where the board is kept in `PROGDIR:wena.sqlite`; Android ARM64
(`wena-android-arm64.apk`) and iOS ARM64 (`wena-ios-arm64.ipa`), where it is
kept in the app's own data folder.

Each start writes `wena-debug-log.txt` beside the executable (on AmigaOS and
AROS `PROGDIR:wena-debug-log.txt`), holding the last run: the system, CPU,
memory and stack where it can tell, SDL's video driver, and every startup
step. A start that fails also prints its last steps where it was started.
`WENA_LOG_DIR` puts the log (`desktop.log`) in that folder instead.

`wena-amigaos-m68k-aga` opens its own 640x512 screen with 256 colors (PAL
hi-res interlaced, the most AGA shows at 8 bits), draws into a 32-bit frame and
maps it onto a palette that keeps WeKan's colors exact. It redraws only after
input. With an RTG card it shows the same 640x512 frame in full color.

The iOS `.ipa` is not signed: iOS installs only signed apps, so re-sign it with
your own certificate, or with AltStore or Sideloadly. The Android `.apk` is
signed with the release key in the repository secrets
`WENA_ANDROID_KEYSTORE_BASE64` (the keystore, base64),
`WENA_ANDROID_KEYSTORE_PASSWORD`, `WENA_ANDROID_KEY_ALIAS` and, when it
differs, `WENA_ANDROID_KEY_PASSWORD`. Keep that key: an APK signed with another
does not install over it. Without the secrets the workflow signs with a
throwaway debug key and warns.

New entries go under `# Upcoming Wena release` in [CHANGELOG.md](CHANGELOG.md).
Menu option 3) Release (`./build.sh release next`) numbers that section after
the newest release (v0.01, v0.02, ... v9.99, v10.00), commits
`Prepare vX release`, pushes, and starts `release-all.yml`. It publishes the
release with the section as its notes, builds and smoke-tests every platform -
natively, under QEMU, in a BSD or Haiku virtual machine, or by cross-compiling -
and attaches each file as soon as its own build has passed, so the release
fills in as builds finish; its last job writes `SHA256SUMS` over them all.
`./build.sh release missing` starts `release-all-missing.yml`, which builds
only the files the newest release lacks (those whose build failed, once fixed)
and adds them, without a new number. Release needs the GitHub CLI logged in and
no uncommitted changes.

`./build.sh build desktop-package` creates a verified local Linux amd64 archive
with the desktop, licenses, checksums and actual host dependency requirements.
SDL2 and SQLite remain system libraries. `wena-desktop --dependency-info` reports
the libraries loaded on the destination machine without opening a workspace.
Use a maintained SQLite build with the documented WAL-reset fix; see the
[dependency review](docs/dependency-audit.md).

Wena code is MIT licensed. Dependency provenance and platform boundaries are
documented under [client](client/README.md), [imports](imports/README.md) and
[server](server/README.md).
