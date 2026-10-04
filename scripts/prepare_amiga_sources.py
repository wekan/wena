#!/usr/bin/env python3
"""Prepare an Amiga-family build's sources on the host, before its container.

    prepare_amiga_sources.py TARGET CACHE_DIR OUTPUT_DIR

The pinned archives are already fetched and checked in CACHE_DIR
(scripts/build_desktop_amiga.sh). This writes, in OUTPUT_DIR:

  sqlite3.c, sqlite3.h   from the SQLite amalgamation, for every target;
  sdl-src/               for the AROS targets: SDL2 2.32.10 with AROS's port
                         applied (its diff, SDL2_static.c and SDL2_intern.h)
                         and Wena's edits - no OpenGL, no system iconv, SDL's
                         own wcslen/wcscmp.

The AROS cross-compiler images (midwan/aros-compiler) carry the compiler and
the SDK and little else - no Python, no patch - so everything that needs them
happens here, and the container only compiles. The same edits as before, for
every AROS CPU.
"""
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
PINS = ROOT / "config" / "release-dependencies.json"
AROS_TARGETS = ("aros-amd64", "aros-i386", "aros-arm64")

# Lines dropped from SDL's AROS configuration and its video driver.
CONFIG_DROPS = ("#define SDL_VIDEO_RENDER_OGL ", "#define SDL_VIDEO_OPENGL ", "#define SDL_VIDEO_OPENGL_AGL ",
                "#define HAVE_ICONV ", "#define HAVE_ICONV_H ", "#define SDL_USE_LIBICONV ",
                "#define HAVE_WCSLEN ", "#define HAVE_WCSCMP ")
VIDEO_DROP = "device->GL_"


def cached(name, cache):
    pin = json.loads(PINS.read_text(encoding="utf-8"))[name]
    path = Path(cache) / pin["url"].rsplit("/", 1)[1]
    if not path.is_file():
        raise SystemExit(f"{name} is not in {cache}: run scripts/fetch_release_dependency.py {name}")
    return path


def sqlite(cache, out):
    with zipfile.ZipFile(cached("sqlite", cache)) as bundle:
        found = 0
        for name in bundle.namelist():
            base = name.rsplit("/", 1)[-1]
            if base in ("sqlite3.c", "sqlite3.h"):
                (out / base).write_bytes(bundle.read(name))
                found += 1
    if found != 2:
        raise SystemExit("the SQLite archive has no sqlite3.c and sqlite3.h")


def drop_lines(path, starts=(), contains=None):
    lines = path.read_text(encoding="utf-8", errors="surrogateescape").splitlines(keepends=True)
    kept = [line for line in lines
            if not any(line.startswith(start) for start in starts)
            and not (contains is not None and contains in line and "= AROS_GL_" in line)]
    path.write_text("".join(kept), encoding="utf-8", errors="surrogateescape")
    return len(lines) - len(kept)


def sdl(cache, out):
    source = out / "sdl-src"
    if source.exists():
        shutil.rmtree(source)
    source.mkdir(parents=True)
    with tarfile.open(cached("sdl2", cache)) as bundle:
        for member in bundle.getmembers():
            parts = member.name.split("/", 1)
            if len(parts) != 2 or not parts[1] or member.issym() or member.islnk():
                continue
            if ".." in Path(parts[1]).parts:
                raise SystemExit(f"unsafe path in the SDL archive: {member.name}")
            member.name = parts[1]
            bundle.extract(member, source)
    with cached("sdl2-aros-patch", cache).open("rb") as diff:
        subprocess.run(["patch", "-p1", "-s"], cwd=source, stdin=diff, check=True)
    shutil.copyfile(cached("sdl2-aros-static", cache), source / "SDL2_static.c")
    shutil.copyfile(cached("sdl2-aros-intern", cache), source / "SDL2_intern.h")
    # No OpenGL: Wena draws with SDL's software renderer, and AROS's libGL.a
    # would tie the executable to gl.library; the driver's GL hooks stay NULL.
    # The SDK has no libiconv and no wcslen/wcscmp; SDL has its own of each.
    if drop_lines(source / "include" / "SDL_config_aros.h", CONFIG_DROPS) == 0:
        raise SystemExit("SDL_config_aros.h has none of the lines Wena drops: the port changed")
    if drop_lines(source / "src" / "video" / "aros" / "SDL_arosvideo.c", contains=VIDEO_DROP) == 0:
        raise SystemExit("SDL_arosvideo.c has no GL hooks to drop: the port changed")


def main(argv):
    if len(argv) != 3:
        print(__doc__.strip().splitlines()[2].strip(), file=sys.stderr)
        return 2
    target, cache, out = argv[0], Path(argv[1]), Path(argv[2])
    out.mkdir(parents=True, exist_ok=True)
    sqlite(cache, out)
    if target in AROS_TARGETS:
        sdl(cache, out)
    print(out)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
