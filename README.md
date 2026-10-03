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

## Tests and existing target builds

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

`build all` builds every release target this computer can build and lists the
ones it cannot, with the reason: macOS and iOS need a Mac (iOS needs Xcode,
which the App Store installs), and the Android NDK runs on amd64 Linux, macOS
and Windows. Linux targets without a compiler here are built in an Ubuntu
container. `install TARGET|all|desktop` installs without building, and
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

The existing cataloged cross-platform `host`/target builds remain bootstrap
executables. `build desktop` is a separate local SDL2/SQLite build. No release
workflow is needed for local development.

## Releases

Each release has one self-contained `wena-desktop` executable per platform,
with SDL2 and SQLite linked in from the checksum-pinned sources in
[config/release-dependencies.json](config/release-dependencies.json):
Linux x86-64, ARM64, ARMv7, ARMv5, x86, RISC-V 64, POWER little-endian,
IBM Z and MIPS64 little-endian; macOS Apple silicon and Intel; and Windows
x86-64, x86 and ARM64. A Linux executable needs only glibc and an X11 or
Wayland desktop, a macOS one only macOS, a Windows one only Windows.
`scripts/build_desktop_release.sh TARGET OUTPUT` builds one, and
`scripts/check_release_executable.py` refuses it when SDL2, SQLite or a
non-system library would be loaded at run time.

New entries go under `# Upcoming Wena release` in [CHANGELOG.md](CHANGELOG.md).
Menu option 3) Release (`./build.sh release next`) numbers that section after
the newest release (v0.01, v0.02, ... v9.99, v10.00), commits
`Prepare vX release`, pushes, and starts `release-desktop.yml` on GitHub. That
workflow publishes the release with the section as its notes, starts
`release-all.yml` for the bootstrap targets, builds and smoke-tests every
platform natively, under QEMU or by cross-compiling, and attaches each
executable with its `.sha256` and the licenses. `./build.sh release missing`
builds and attaches to the newest release without a new number. Release needs
the GitHub CLI logged in and no uncommitted changes.

`./build.sh build desktop-package` creates a verified local Linux amd64 archive
with the desktop, licenses, checksums and actual host dependency requirements.
SDL2 and SQLite remain system libraries. `wena-desktop --dependency-info` reports
the libraries loaded on the destination machine without opening a workspace.
Use a maintained SQLite build with the documented WAL-reset fix; see the
[dependency review](docs/dependency-audit.md).

Wena code is MIT licensed. Dependency provenance and platform boundaries are
documented under [client](client/README.md), [imports](imports/README.md) and
[server](server/README.md).
