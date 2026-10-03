#!/usr/bin/env python3
"""The AmigaOS 4, AROS and AmigaOS 3 desktop builds, checked without Docker.

scripts/build_desktop_amiga.sh compiles inside pinned amigadev/crosstools
images; this suite checks what it relies on: every image and download is
pinned in config/release-dependencies.json, the scripts take them from there,
the C sources have their Amiga branches, the host build is unchanged, and an
unknown target is refused. When the pinned SQLite amalgamation is already in
.tools/cache and a C compiler is present, it also compiles SQLite with the
Amiga options and the "amiga" VFS and runs Wena's own pragmas against it.
"""

import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
HOST = ROOT / "scripts" / "build_desktop_amiga.sh"
CONTAINER = ROOT / "scripts" / "build_desktop_amiga_container.sh"
PINS = json.loads((ROOT / "config" / "release-dependencies.json").read_text(encoding="utf-8"))
TARGETS = ("amigaos4-ppc", "aros-amd64", "amigaos-m68k")
AMIGA = "defined(__amigaos__) || defined(__AROS__)"


def read(relative):
    return (ROOT / relative).read_text(encoding="utf-8")


def test_images_pinned_by_digest():
    images = {entry["target"]: entry["image"] for entry in PINS["docker-images"]}
    assert sorted(images) == sorted(TARGETS)
    for target, image in images.items():
        assert re.fullmatch(r"amigadev/crosstools:[a-z0-9._-]+@sha256:[0-9a-f]{64}", image), target
    assert images["aros-amd64"].endswith(
        "@sha256:9c4e978301da6b6584d68d4014caa1fa9e2689529e79e71f7190d40d1cb62e50")
    assert images["amigaos4-ppc"].startswith("amigadev/crosstools:ppc-amigaos@")
    assert images["amigaos-m68k"].startswith("amigadev/crosstools:m68k-amigaos-gcc10@")
    # The scripts name no image of their own: the digest lives in one place.
    for script in (HOST, CONTAINER):
        assert not re.search(r"sha256:[0-9a-f]{64}", script.read_text(encoding="utf-8")), script.name
    assert '"docker-images"' in HOST.read_text(encoding="utf-8")
    # AROS's image is amd64 only; the others run natively on arm64 hosts.
    assert 'aros-amd64) sources="sqlite sdl2 sdl2-aros-patch sdl2-aros-static sdl2-aros-intern"; platform=linux/amd64' \
        in HOST.read_text(encoding="utf-8")


def test_sources_pinned():
    contrib = r"https://raw\.githubusercontent\.com/aros-development-team/contrib/[0-9a-f]{40}/SDL2/main/"
    for name, file in (("sdl2-aros-patch", "SDL2-2.32.10-aros.diff"), ("sdl2-aros-static", "SDL2_static.c"),
                       ("sdl2-aros-intern", "SDL2_intern.h")):
        assert re.fullmatch(contrib + re.escape(file), PINS[name]["url"]), name
    commits = {re.search(r"contrib/([0-9a-f]{40})/", PINS[name]["url"])[1]
               for name in ("sdl2-aros-patch", "sdl2-aros-static", "sdl2-aros-intern")}
    assert len(commits) == 1, "the AROS port files come from one commit"
    assert PINS["sdl2"]["version"] == "2.32.10", "the AROS patch is for SDL2 2.32.10"
    fork = PINS["sdl2-amigaos3"]
    assert fork["url"] == "https://github.com/diasurgical/SDL/archive/240bc700a9408171edad309590ec180e67c42aeb.tar.gz"
    assert fork["sha256"] == "f05477fb63a50b99e855e84daa10c13188b10b6cfb7653c7c37090e1ee689e69"
    assert "220f9ea9e94c3e2c6a5054822c05c08f" in fork["verified"], "DevilutionX's MD5 pin"
    for name in ("sdl2-aros-patch", "sdl2-aros-static", "sdl2-aros-intern", "sdl2-amigaos3", "sqlite", "sdl2"):
        pin = PINS[name]
        assert re.fullmatch(r"[0-9a-f]{64}", pin["sha256"]) and pin["license"] and pin["verified"], name
    # Every download either script asks for is pinned, and fetched only
    # through fetch_release_dependency.py.
    host, container = HOST.read_text(encoding="utf-8"), CONTAINER.read_text(encoding="utf-8")
    for listed in re.findall(r'sources="([^"]+)"', host):
        for name in listed.split():
            assert isinstance(PINS.get(name), dict), name
    for name in re.findall(r'fetch ([a-z0-9-]+)\)', container):
        assert isinstance(PINS.get(name), dict), name
    for script in (host, container):
        assert not re.search(r"\b(curl|wget)\b", script)
        assert "fetch_release_dependency.py" in script


def test_container_build():
    script = CONTAINER.read_text(encoding="utf-8")
    # Wena's sources through the one source list, with the strict flags.
    assert 'sh "$root_dir/scripts/build_desktop.sh" "$output"' in script
    assert "WENA_SQLITE_LIBS=\"$sqlite_dir/sqlite3.o $sqlite_dir/sqlite_amiga_vfs.o\"" in script
    # SDL2: the image's static archive, AROS's mmakefile list, the m68k fork.
    assert "sdl_libs=/opt/ppc-amigaos/usr/lib/libSDL2.a" in script
    assert "-DSDL2_AROS_STATIC" in script and "SDL2_static" in script
    assert "render/opengl" not in script.replace("less render/opengl", "")
    assert "SDL_arosopengl" not in script.replace("and SDL_arosopengl", "")
    for option in ("-DM68K_CPU=68040", "-DM68K_FPU=hard", '"-DM68K_COMMON=-s -fbbb=- -ffast-math"',
                   "-DSDL_STATIC=ON", "-DSDL_SHARED=OFF", "-DSDL_AMIGAOS3_AGA=OFF"):
        assert option in script, option
    # The stack: libnix's swapstack module is linked for __stack.
    assert "-Wl,-u,___stkswap" in script
    # The proof at the end: format and no shared objects.
    assert "000003f3" in script and "7f454c460102" in script and "7f454c460201" in script
    assert "readelf -d" in script


def test_c_platform_branches():
    for relative in ("client/platform/files.c", "client/desktop.c", "server/executable_path.c",
                     "server/sqlite_workspace.c", "imports/i18n/locale.c", "imports/i18n/language.c",
                     "imports/preferences/collapse.c"):
        assert AMIGA in read(relative), relative
    debug_log = read("client/platform/debug_log.c")
    assert "WENA_SYSTEM_AMIGA" in debug_log and '"PROGDIR:wena.sqlite"' in debug_log
    desktop = read("client/desktop.c")
    assert "#define DESKTOP_SYSTEM WENA_SYSTEM_AMIGA" in desktop
    assert '"$STACK:1048576"' in desktop and "unsigned long __stack = DESKTOP_STACK;" in desktop
    assert "#define DESKTOP_STACK 1048576UL" in desktop and "NewStackSwap(" in desktop
    workspace = read("server/sqlite_workspace.c")
    assert "Rename((CONST_STRPTR)staging, (CONST_STRPTR)path)" in workspace
    assert "IDOS->" not in workspace and "#define __USE_INLINE__" in workspace
    assert "OpenLocale(NULL)" in read("imports/i18n/locale.c")
    vfs = read("server/sqlite_amiga_vfs.c")
    assert 'sqlite3_vfs_find("unix-none")' in vfs and "sqlite3_vfs_register(&amiga_vfs, 1)" in vfs
    # Wena's own sources stay strict C89 on these targets too: the container
    # changes headers' feature macros, never the language.
    script = CONTAINER.read_text(encoding="utf-8")
    assert "-std=gnu" not in script.replace("-std=gnu99 -O2 -DSDL2_AROS_STATIC", "")
    build = read("scripts/build_desktop.sh")
    assert "-std=c89 -pedantic-errors -Wall -Wextra -Werror" in build


def test_host_build_unchanged():
    build = read("scripts/build_desktop.sh")
    checks = read("scripts/check_desktop_sources.sh")
    # Every other build still runs the source checks; only the Amiga host
    # script, which has run them itself, skips them in its container.
    assert '[ "${WENA_SOURCES_CHECKED:-}" = 1 ] || sh "$root_dir/scripts/check_desktop_sources.sh"' in build
    for check in ("check_dependencies.py", "compile_svg.py\" --check", "verify_migrations.py",
                  "verify_i18n_catalog.py", "generate_ui_i18n.py\" --check", "generate_native_font.py\" --check"):
        assert check in checks, check
        assert check not in build, check
    host = HOST.read_text(encoding="utf-8")
    assert 'sh "$root_dir/scripts/check_desktop_sources.sh"' in host
    assert host.index("check_desktop_sources.sh") < host.index("docker run")
    assert "--env WENA_SOURCES_CHECKED=1" in host
    assert "WENA_SOURCES_CHECKED" not in read("scripts/build_desktop_release.sh")


def run(command, env=None):
    return subprocess.run(command, cwd=ROOT, env=env, text=True, capture_output=True, timeout=60)


def test_refuses_unknown_target():
    sh = shutil.which("sh")
    for script in (HOST, CONTAINER):
        for target in ("amigaos-ppc", "aros-i386", "linux-amd64", ""):
            with tempfile.TemporaryDirectory() as temp:
                result = run([sh, str(script), target, str(Path(temp) / "wena")])
                assert result.returncode == 2, (script.name, target, result.stderr)
                assert "unknown Amiga desktop target" in result.stderr
                assert not list(Path(temp).iterdir())
        result = run([sh, str(script), "aros-amd64"])
        assert result.returncode == 2 and "Usage:" in result.stderr
    assert not (ROOT / ".tools" / "release" / "aros-i386").exists()
    # Without Docker the host script stops before fetching anything.
    with tempfile.TemporaryDirectory() as temp:
        for tool in ("dirname",):
            os.symlink(shutil.which(tool), Path(temp) / tool)
        result = run([sh, str(HOST), "aros-amd64", str(Path(temp) / "wena")], env={"PATH": temp})
        assert result.returncode == 1 and "Docker is required" in result.stderr, result.stderr


PROBE = r'''
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
static char mode[32];
static int keep(void *unused, int n, char **values, char **names)
{
    (void)unused; (void)names;
    if (n == 1 && values[0] != NULL && strlen(values[0]) < sizeof(mode)) strcpy(mode, values[0]);
    return 0;
}
int main(int argc, char **argv)
{
    sqlite3 *db;
    int ok;
    (void)argc;
    if (sqlite3_open_v2(argv[1], &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                        SQLITE_OPEN_FULLMUTEX, NULL) != SQLITE_OK) return 2;
    if (strcmp(sqlite3_vfs_find(NULL)->zName, "amiga") != 0) return 3;
    /* What wena_sqlite_open and wena_sqlite_workspace_create run. */
    if (sqlite3_exec(db, "PRAGMA foreign_keys=ON;PRAGMA journal_mode=WAL;PRAGMA synchronous=FULL;",
                     keep, NULL, NULL) != SQLITE_OK || strcmp(mode, "delete") != 0) return 4;
    ok = sqlite3_exec(db, "CREATE TABLE t(x);BEGIN IMMEDIATE;"
        "WITH RECURSIVE c(i) AS (SELECT 1 UNION ALL SELECT i+1 FROM c WHERE i<50000) "
        "INSERT INTO t SELECT randomblob(64) FROM c;COMMIT;CREATE INDEX tx ON t(x);"
        "PRAGMA wal_checkpoint(TRUNCATE); PRAGMA journal_mode=DELETE;", NULL, NULL, NULL) == SQLITE_OK;
    if (sqlite3_close(db) != SQLITE_OK || !ok) return 5;
    puts("amiga sqlite ok");
    return 0;
}
'''


def test_sqlite_options_with_wal_requests():
    cc = os.environ.get("CC") or shutil.which("cc")
    pin = PINS["sqlite"]
    archive = ROOT / ".tools" / "cache" / pin["url"].rsplit("/", 1)[1]
    if not cc or not archive.is_file() or hashlib.sha256(archive.read_bytes()).hexdigest() != pin["sha256"]:
        print("skipped the SQLite probe: no C compiler or no pinned amalgamation in .tools/cache")
        return
    script = CONTAINER.read_text(encoding="utf-8").replace("\\\n", " ")
    line = next(line for line in script.splitlines() if '-c "$sqlite_dir/sqlite3.c"' in line)
    defines = [token for token in line.split() if token.startswith("-D")]
    assert "-DSQLITE_OMIT_WAL" in defines and "-DSQLITE_EXTRA_INIT=wena_sqlite_amiga_init" in defines
    with tempfile.TemporaryDirectory() as temp:
        temp = Path(temp)
        with zipfile.ZipFile(archive) as bundle:
            for name in bundle.namelist():
                if name.rsplit("/", 1)[-1] in {"sqlite3.c", "sqlite3.h"}:
                    (temp / name.rsplit("/", 1)[-1]).write_bytes(bundle.read(name))
        (temp / "probe.c").write_text(PROBE, encoding="utf-8")
        subprocess.run([cc, "-c", *defines, str(temp / "sqlite3.c"), "-o", str(temp / "sqlite3.o")],
                       check=True, timeout=600)
        subprocess.run([cc, "-std=c89", "-pedantic-errors", "-Wall", "-Wextra", "-Werror", "-isystem", str(temp),
                        "-c", str(ROOT / "server" / "sqlite_amiga_vfs.c"), "-o", str(temp / "vfs.o")],
                       check=True, timeout=120)
        subprocess.run([cc, "-isystem", str(temp), str(temp / "probe.c"), str(temp / "sqlite3.o"),
                        str(temp / "vfs.o"), "-o", str(temp / "probe")], check=True, timeout=120)
        database = temp / "board.sqlite"
        result = subprocess.run([str(temp / "probe"), str(database)], text=True, capture_output=True, timeout=120)
        assert result.returncode == 0 and "amiga sqlite ok" in result.stdout, (result.returncode, result.stderr)
        # A standalone file: no WAL, no shared memory, no temporary files left.
        assert sorted(path.name for path in temp.iterdir() if path.name.startswith("board")) == ["board.sqlite"]


def main():
    for test in (test_images_pinned_by_digest, test_sources_pinned, test_container_build,
                 test_c_platform_branches, test_host_build_unchanged, test_refuses_unknown_target,
                 test_sqlite_options_with_wal_requests):
        test()
    print("Amiga desktop build checks passed")


if __name__ == "__main__":
    main()
