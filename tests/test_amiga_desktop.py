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
TARGETS = ("amigaos4-ppc", "aros-amd64", "amigaos-m68k", "amigaos-m68k-aga")
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
    # The AGA build is the same toolchain: one image, pinned once per target.
    assert images["amigaos-m68k-aga"] == images["amigaos-m68k"]
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
    # AGA: Wena's patch on the pinned fork, the c2p sized for 640x512 when
    # vasm is there, and the desktop's own AGA frame.
    assert 'patch -p1 -s < "$root_dir/scripts/patches/sdl2-amigaos3-aga.patch"' in script
    assert "-DSDL_AMIGAOS3_AGA=ON -DSDL_AMIGAOS3_C2P_BPLSIZE=40960" in script
    assert 'cflags="$cflags -DWENA_AMIGA_AGA=1"' in script
    assert script.index("sdl2-amigaos3-aga.patch") < script.index("cmake -S")
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


LIBNIX_LSEEK = r'''
/* libnix's lseek() on AmigaDOS, which cannot Seek() past the end of a file:
 * it seeks to the end and writes the gap from a buffer it has not cleared.
 * Here the gap is filled with 'J', so stray bytes are easy to see. */
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
off_t wena_test_libnix_lseek(int descriptor, off_t offset, int whence)
{
    if (whence == SEEK_SET) {
        off_t end = lseek(descriptor, 0, SEEK_END);
        char junk[512];
        memset(junk, 'J', sizeof(junk));
        while (end >= 0 && end < offset) {
            size_t part = offset - end > (off_t)sizeof(junk) ? sizeof(junk) : (size_t)(offset - end);
            if (write(descriptor, junk, part) <= 0) return -1;
            end += (off_t)part;
        }
    }
    return lseek(descriptor, offset, whence);
}

/* SQLite's own way without pread() and pwrite(), which is what the Amiga
 * builds got: lseek() to the offset, then read() or write(). */
ssize_t wena_test_seek_pread(int descriptor, void *buffer, size_t count, off_t offset)
{
    if (wena_test_libnix_lseek(descriptor, offset, SEEK_SET) != offset) return -1;
    return read(descriptor, buffer, count);
}

ssize_t wena_test_seek_pwrite(int descriptor, const void *buffer, size_t count, off_t offset)
{
    if (wena_test_libnix_lseek(descriptor, offset, SEEK_SET) != offset) return -1;
    return write(descriptor, buffer, count);
}
'''

LIBNIX_LSEEK_H = """#include <sys/types.h>
off_t wena_test_libnix_lseek(int descriptor, off_t offset, int whence);
ssize_t wena_test_seek_pread(int descriptor, void *buffer, size_t count, off_t offset);
ssize_t wena_test_seek_pwrite(int descriptor, const void *buffer, size_t count, off_t offset);
"""

ATTACH_PROBE = r'''
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
/* What wena_wekan_sync_attach does with a new wekan.sqlite: attach it to an
 * in-memory database, make tables, write rows; then the file must be a
 * database that opens again and passes integrity_check. */
int main(int argc, char **argv)
{
    sqlite3 *db;
    sqlite3_stmt *statement;
    char header[16];
    FILE *file;
    int ok;
    (void)argc;
    if (sqlite3_open(":memory:", &db) != SQLITE_OK) return 2;
    if (sqlite3_prepare_v2(db, "ATTACH DATABASE ?1 AS fdb", -1, &statement, NULL) != SQLITE_OK) return 3;
    sqlite3_bind_text(statement, 1, argv[1], -1, SQLITE_TRANSIENT);
    ok = sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    if (!ok) { fprintf(stderr, "attach: %s\n", sqlite3_errmsg(db)); return 4; }
    if (sqlite3_exec(db, "PRAGMA fdb.journal_mode=WAL; CREATE TABLE fdb.t(x TEXT NOT NULL) STRICT;"
                     "WITH RECURSIVE c(i) AS (SELECT 1 UNION ALL SELECT i+1 FROM c WHERE i<3000) "
                     "INSERT INTO fdb.t SELECT hex(randomblob(40)) FROM c;", NULL, NULL, NULL) != SQLITE_OK) {
        fprintf(stderr, "write: %s\n", sqlite3_errmsg(db)); return 5;
    }
    sqlite3_close(db);
    file = fopen(argv[1], "rb");
    if (file == NULL || fread(header, 1, 16, file) != 16) return 6;
    fclose(file);
    if (memcmp(header, "SQLite format 3", 16) != 0) { fprintf(stderr, "header: %.16s\n", header); return 7; }
    if (sqlite3_open(argv[1], &db) != SQLITE_OK) return 8;
    if (sqlite3_prepare_v2(db, "PRAGMA integrity_check", -1, &statement, NULL) != SQLITE_OK) return 9;
    ok = sqlite3_step(statement) == SQLITE_ROW && !strcmp((const char *)sqlite3_column_text(statement, 0), "ok");
    sqlite3_finalize(statement);
    sqlite3_close(db);
    if (!ok) return 10;
    puts("attached file ok");
    return 0;
}
'''


def test_sqlite_never_seeks_past_the_end():
    """AmigaDOS cannot seek past a file's end, and libnix's lseek() fills the
    gap with stray bytes: a new wekan.sqlite became 24 bytes of garbage
    (wena6 log, traced with vamos). The Amiga SQLite reads and writes at an
    offset through sqlite_amiga_vfs.c instead, and a fresh attached file is a
    sound database even with libnix's lseek() - while the same build without
    those options reproduces the fault."""
    cc = os.environ.get("CC") or shutil.which("cc")
    pin = PINS["sqlite"]
    archive = ROOT / ".tools" / "cache" / pin["url"].rsplit("/", 1)[1]
    if not cc or not archive.is_file() or hashlib.sha256(archive.read_bytes()).hexdigest() != pin["sha256"]:
        print("skipped the libnix lseek probe: no C compiler or no pinned amalgamation in .tools/cache")
        return
    script = CONTAINER.read_text(encoding="utf-8").replace("\\\n", " ")
    line = next(line for line in script.splitlines() if '-c "$sqlite_dir/sqlite3.c"' in line)
    tokens = line.replace('"$root_dir', '"' + str(ROOT)).replace('"', "").split()
    options = []
    for index, token in enumerate(tokens):
        if token.startswith("-D"):
            options.append(token)
        elif token == "-include":
            options += [token, tokens[index + 1]]
    for needed in ("-DUSE_PREAD", "-Dpread=wena_sqlite_pread", "-Dpwrite=wena_sqlite_pwrite", "-include"):
        assert needed in options, needed
    # This host's SQLite always uses pread() and pwrite(); the Amiga's had
    # neither and seeked instead, which these stand for.
    without = [o for o in options if o not in ("-DUSE_PREAD", "-Dpread=wena_sqlite_pread",
                                               "-Dpwrite=wena_sqlite_pwrite", "-include")
               and not o.endswith("sqlite_amiga_io.h")]
    without += ["-Dpread=wena_test_seek_pread", "-Dpwrite=wena_test_seek_pwrite"]
    with tempfile.TemporaryDirectory(dir=os.environ.get("TMPDIR")) as temp:
        temp = Path(temp)
        with zipfile.ZipFile(archive) as bundle:
            for name in bundle.namelist():
                if name.rsplit("/", 1)[-1] in {"sqlite3.c", "sqlite3.h"}:
                    (temp / name.rsplit("/", 1)[-1]).write_bytes(bundle.read(name))
        (temp / "libnix_lseek.c").write_text(LIBNIX_LSEEK, encoding="utf-8")
        (temp / "libnix_lseek.h").write_text(LIBNIX_LSEEK_H, encoding="utf-8")
        (temp / "probe.c").write_text(ATTACH_PROBE, encoding="utf-8")
        libnix = ["-include", str(temp / "libnix_lseek.h"), "-Dlseek=wena_test_libnix_lseek"]
        subprocess.run([cc, "-c", str(temp / "libnix_lseek.c"), "-o", str(temp / "libnix.o")], check=True)
        subprocess.run([cc, "-c", str(temp / "probe.c"), "-isystem", str(temp), "-o", str(temp / "probe.o")],
                       check=True)
        subprocess.run([cc, "-std=c89", "-pedantic-errors", "-Wall", "-Wextra", "-Werror", "-isystem", str(temp),
                        *libnix, "-c", str(ROOT / "server" / "sqlite_amiga_vfs.c"), "-o", str(temp / "vfs.o")],
                       check=True, timeout=120)
        results = {}
        for kind, flags in (("fixed", options), ("unfixed", without)):
            subprocess.run([cc, "-c", "-w", *flags, *libnix, str(temp / "sqlite3.c"), "-o", str(temp / "sqlite3.o")],
                           check=True, timeout=600)
            subprocess.run([cc, str(temp / "probe.o"), str(temp / "sqlite3.o"), str(temp / "vfs.o"),
                            str(temp / "libnix.o"), "-o", str(temp / ("probe-" + kind))], check=True, timeout=120)
            database = temp / (kind + ".sqlite")
            results[kind] = subprocess.run([str(temp / ("probe-" + kind)), str(database)], text=True,
                                           capture_output=True, timeout=120)
        fixed, unfixed = results["fixed"], results["unfixed"]
        assert fixed.returncode == 0 and "attached file ok" in fixed.stdout, (fixed.returncode, fixed.stderr)
        assert b"J" * 16 not in (temp / "fixed.sqlite").read_bytes()
        # Negative: without them, libnix's lseek() spoils the new file, as on
        # the Amiga - the probe above would catch a return of the fault.
        assert unfixed.returncode != 0, ("the libnix lseek model no longer reproduces the fault", unfixed.stdout)


def test_window_patch():
    """AmigaOS 3 in a Workbench window (wena7 screenshot): the fork wrote the
    frame into the SCREEN's bitmap at its top-left corner, and every update was
    taken as one to scale, since the outer width (borders included) is always
    wider than the frame. The window patch draws into the window's own
    RastPort - placed and clipped by the layers - and takes the mouse from the
    inner area. Both m68k builds get it."""
    patch = (ROOT / "scripts" / "patches" / "sdl2-amigaos3-window.patch").read_text(encoding="utf-8")
    assert re.findall(r"^\+\+\+ b/(\S+)$", patch, re.M) == ["src/video/amigaos3/SDL_os3framebuffer.c",
                                                          "src/video/amigaos3/SDL_os3events.c"]
    assert not re.search(r"^(---|\+\+\+) \S+[ \t]", patch, re.M), "no timestamps"
    added = "\n".join(line[1:] for line in patch.splitlines() if line.startswith("+"))
    windowed = added[added.index("if (!data->is_fullscreen) {"):added.index("return 0;", added.index("if (!data->is_fullscreen) {"))]
    assert "WritePixelArray(" in windowed and "win->RPort" in windowed
    assert "win->GZZWidth" in windowed and "win->GZZHeight" in windowed, "clipped to the inner area"
    # Negative: the windowed path never touches the screen's bitmap itself.
    for direct in ("LockBitMapTags", "BitMapScale", "RPort->BitMap", "->Width", "->Height"):
        assert direct not in windowed, direct
    assert "intuiwin->GZZMouseX" in added and "intuiwin->GZZMouseY" in added and "WFLG_GIMMEZEROZERO" in added
    # Both builds, before CMake; a kept SDL is rebuilt when a patch changes.
    script = CONTAINER.read_text(encoding="utf-8")
    window_at = script.index('patch -p1 -s < "$root_dir/scripts/patches/sdl2-amigaos3-window.patch"')
    assert window_at < script.index('if [ "$target" = amigaos-m68k-aga ]; then\n        (cd') < script.index("cmake -S")
    assert 'patches.sha256' in script and "sdl2-amigaos3-*.patch | sha256sum" in script
    # It applies to the pinned fork, alone and before the AGA patch.
    cache = ROOT / ".tools" / "cache"
    pin = PINS["sdl2-amigaos3"]
    archive = cache / pin["url"].rsplit("/", 1)[1]
    if not archive.is_file() or hashlib.sha256(archive.read_bytes()).hexdigest() != pin["sha256"] or not shutil.which("patch"):
        print("skipped applying the window patch: no pinned fork in .tools/cache")
        return
    import tarfile
    wanted = ("CMakeLists.txt", "src/video/amigaos3/SDL_os3aga.c", "src/video/amigaos3/SDL_os3framebuffer.c",
              "src/video/amigaos3/SDL_os3events.c")
    with tempfile.TemporaryDirectory(dir=os.environ.get("TMPDIR")) as temp:
        with tarfile.open(archive) as bundle:
            for member in bundle.getmembers():
                parts = member.name.split("/", 1)
                if len(parts) == 2 and parts[1] in wanted:
                    member.name = parts[1]
                    bundle.extract(member, temp)
        for name in ("sdl2-amigaos3-window.patch", "sdl2-amigaos3-aga.patch"):
            result = subprocess.run(["patch", "-p1", "-s"], cwd=temp, capture_output=True,
                                    stdin=(ROOT / "scripts" / "patches" / name).open("rb"))
            assert result.returncode == 0, (name, result.stdout, result.stderr)
        framebuffer = (Path(temp) / "src/video/amigaos3/SDL_os3framebuffer.c").read_text(encoding="utf-8")
        assert framebuffer.index("if (!data->is_fullscreen) {") < framebuffer.index("/* --- RTG path (our own screen) --- */")


def main():
    for test in (test_images_pinned_by_digest, test_sources_pinned, test_container_build,
                 test_c_platform_branches, test_host_build_unchanged, test_refuses_unknown_target,
                 test_sqlite_options_with_wal_requests, test_sqlite_never_seeks_past_the_end, test_window_patch):
        test()
    print("Amiga desktop build checks passed")


if __name__ == "__main__":
    main()
