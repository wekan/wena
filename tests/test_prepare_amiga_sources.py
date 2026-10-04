#!/usr/bin/env python3
"""scripts/prepare_amiga_sources.py, which readies an Amiga build's sources
on the host so the AROS images (midwan/aros-compiler: a compiler and an SDK,
no Python, no patch) only compile: SQLite for every target, and for every
AROS CPU SDL2 with AROS's port applied and Wena's edits made."""
import importlib.util
import os
from pathlib import Path
import shutil
import tempfile

ROOT = Path(__file__).resolve().parents[1]
CACHE = ROOT / ".tools" / "cache"


def module():
    spec = importlib.util.spec_from_file_location("prepare", ROOT / "scripts" / "prepare_amiga_sources.py")
    loaded = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(loaded)
    return loaded


def main():
    prepare = module()
    assert prepare.AROS_TARGETS == ("aros-amd64", "aros-i386", "aros-arm64")
    # The host script prepares before Docker; the container only copies.
    host = (ROOT / "scripts" / "build_desktop_amiga.sh").read_text(encoding="utf-8")
    container = (ROOT / "scripts" / "build_desktop_amiga_container.sh").read_text(encoding="utf-8")
    assert host.index("prepare_amiga_sources.py") < host.index("docker run")
    assert 'cp -R "$work/sources/sdl-src" "$work/sdl-src"' in container
    assert 'cp "$work/sources/sqlite3.c" "$work/sources/sqlite3.h" "$sqlite_dir/"' in container
    aros = container[container.index("  aros-amd64|aros-i386|aros-arm64)\n    if [ ! -f"):]
    aros = aros[:aros.index("    ;;")]
    # Negative: nothing in the AROS container path needs Python, patch or sed.
    for tool in ("python3", "patch ", "fetch ", "sed "):
        assert tool not in aros, tool
    with tempfile.TemporaryDirectory(dir=os.environ.get("TMPDIR")) as temp:
        temp = Path(temp)
        # Negative: an archive that is not in the cache stops it.
        try:
            prepare.main(["aros-i386", str(temp / "empty"), str(temp / "out")])
        except SystemExit as stop:
            assert "is not in" in str(stop)
        else:
            raise AssertionError("prepared without the archives")
        needed = ("sqlite-amalgamation", "SDL2-2.32.10.tar.gz", "SDL2-2.32.10-aros.diff", "SDL2_static.c", "SDL2_intern.h")
        if not CACHE.is_dir() or not all(any(path.name.startswith(n) for path in CACHE.iterdir()) for n in needed) \
                or not shutil.which("patch"):
            print("skipped preparing: the pinned archives are not in .tools/cache")
            return
        for target in ("amigaos-m68k", "aros-arm64"):
            out = temp / target
            assert prepare.main([target, str(CACHE), str(out)]) == 0
            assert (out / "sqlite3.c").stat().st_size > 1000000 and (out / "sqlite3.h").is_file()
            assert (out / "sdl-src").exists() == (target == "aros-arm64"), target
        source = temp / "aros-arm64" / "sdl-src"
        config = (source / "include" / "SDL_config_aros.h").read_text(encoding="utf-8")
        assert "#define SDL_VIDEO_OPENGL " not in config and "#define HAVE_ICONV " not in config
        assert "#define SDL_VIDEO_DRIVER_AROS" in config or "AROS" in config
        video = (source / "src" / "video" / "aros" / "SDL_arosvideo.c").read_text(encoding="utf-8")
        assert "= AROS_GL_" not in video
        assert (source / "SDL2_static.c").is_file() and (source / "SDL2_intern.h").is_file()
        # Again: the tree is made afresh, not patched twice.
        assert prepare.main(["aros-arm64", str(CACHE), str(temp / "aros-arm64")]) == 0
        # Negative: a port without the lines Wena drops is refused, not built
        # as some other SDL.
        drop_lines = prepare.drop_lines
        prepare.drop_lines = lambda path, starts=(), contains=None: 0
        try:
            prepare.sdl(CACHE, temp / "changed")
        except SystemExit as stop:
            assert "the port changed" in str(stop)
        else:
            raise AssertionError("a changed port was prepared")
        finally:
            prepare.drop_lines = drop_lines
    print("prepare_amiga_sources: SQLite for all, AROS's SDL2 port for every AROS CPU, nothing for the container")


if __name__ == "__main__":
    main()
