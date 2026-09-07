# Third-party dependencies

`nuklear/` is the upstream Nuklear repository pinned as a Git submodule. Wena
chooses Nuklear's MIT license alternative, recorded in `nuklear/LICENSE`. The SDL2
renderer backend is compiled through `client/platform/sdl_nuklear.c`.

Clone with submodules or initialize it after cloning:

```sh
git submodule update --init --recursive
```

The exact compiled Nuklear header, SDL2 renderer backend and license are checked
against `config/dependencies-lock.json` by `scripts/check_dependencies.py`.
Run it after submodule initialization and before compiling the desktop. Optional
`--runtime` reports linked host SDL2/SQLite versions without fetching libraries.
See `docs/dependency-audit.md` for the current scoped review, the SQLite WAL-reset
fix requirement for concurrent deployments, and the font-input boundary.

`fonts/` contains the unchanged, pinned Roboto static font, its Apache-2.0
NOTICE and provenance. The desktop embeds its bytes; the offline generator
verifies the asset without adding a font-file runtime dependency. See
[font provenance and coverage](fonts/README.md).
