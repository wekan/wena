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
./dist/desktop/wena-desktop --database /absolute/path/wena.sqlite --actor ACTOR_ID --board BOARD_ID
```

The desktop entry point opens an **existing Wena relational database** and an
existing actor/board. It displays the board hierarchy and connects card title
editing and archiving to the transactional SQLite adapter. Swimlanes and lists
can be collapsed and expanded. Local database access trusts the operating-system
user; the actor argument is not a login mechanism.

First-run board creation, complete translations, remote synchronization and the
remaining WeKan features are still pending. Do not point this command at a
Meteor/FerretDB database: direct format migration is not implemented. See
[native desktop details](docs/native-desktop.md) for setup and limitations.

## Tests and existing target builds

```sh
./build.sh tests all
./build.sh tests --list
./build.sh tests card-editor-sqlite
./build.sh tests nuklear-editor
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

Wena code is MIT licensed. Dependency provenance and platform boundaries are
documented under [client](client/README.md), [imports](imports/README.md) and
[server](server/README.md).
