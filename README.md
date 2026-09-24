# Wena — WeKan Native Kanban

Wena is an in-progress native WeKan implementation using C89, SDL2, Nuklear
and SQLite. It is not yet a complete WeKan replacement. [ROADMAP.md](ROADMAP.md)
records tested slices, remaining work and the next resume checkpoint.

## Local desktop development

Install a C compiler, Python 3, SDL2 development files and SQLite development
files, then initialize the pinned MIT-licensed Nuklear source:

```sh
git submodule update --init
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
remembering collapsed lists and swimlanes per local actor and board. Changes use guarded SQLite transactions and survive reopening.
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
```

The test runner executes independent native suites in parallel, serializes
shared artifact builds, and reports failures and missing prerequisites
separately. Node.js enables the optional JavaScript runtime suite. Set
`WEKAN_ROOT` to the canonical WeKan checkout pinned by the parity tests when
running upstream source checks.

The existing cataloged cross-platform `host`/target builds remain bootstrap
executables; they do not yet package the desktop application. `build desktop`
is a separate local SDL2/SQLite build. No release workflow is needed for local
development.

`./build.sh build desktop-package` creates a verified local Linux amd64 archive
with the desktop, licenses, checksums and actual host dependency requirements.
SDL2 and SQLite remain system libraries. `wena-desktop --dependency-info` reports
the libraries loaded on the destination machine without opening a workspace.
Use a maintained SQLite build with the documented WAL-reset fix; see the
[dependency review](docs/dependency-audit.md).

Wena code is MIT licensed. Dependency provenance and platform boundaries are
documented under [client](client/README.md), [imports](imports/README.md) and
[server](server/README.md).
