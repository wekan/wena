# Third-party dependencies

`nuklear/` is the upstream Nuklear repository pinned as a Git submodule. Wena
chooses Nuklear's MIT license alternative, recorded in `nuklear/LICENSE`. The SDL2
renderer backend is compiled through `client/platform/sdl_nuklear.c`.

Clone with submodules or initialize it after cloning:

```sh
git submodule update --init --recursive
```
