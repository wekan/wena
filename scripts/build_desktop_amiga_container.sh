#!/bin/sh
# Inside the pinned amigadev/crosstools image of TARGET (started by
# scripts/build_desktop_amiga.sh, repository at /work): build a static SDL2
# where the image has none, compile the SQLite amalgamation, and link the
# desktop with scripts/build_desktop.sh. Wena's own sources keep
# -std=c89 -pedantic-errors -Werror; SDL2 and SQLite get what they need.
#
#   sh scripts/build_desktop_amiga_container.sh TARGET OUTPUT_EXECUTABLE
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if [ "$#" -ne 2 ]; then
  echo "Usage: scripts/build_desktop_amiga_container.sh TARGET OUTPUT_EXECUTABLE" >&2
  exit 2
fi
target=$1
output=$2
work="$root_dir/.tools/release/$target"
cache="$root_dir/.tools/cache"
jobs=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)
fetch() { python3 "$root_dir/scripts/fetch_release_dependency.py" "$1" "$cache"; }
case "$target" in
  amigaos4-ppc)
    # newlib, the C library AmigaOS 4 ships as newlib.library; -static keeps
    # every ELF shared object (SDL2-2.30.so, libc.so) out of the executable.
    # This newlib declares fdopen(), fileno() and the rest of POSIX only
    # without __STRICT_ANSI__, which -std=c89 defines; the language stays C89.
    cc=ppc-amigaos-gcc
    cflags=-U__STRICT_ANSI__
    # Stripped after linking: this ld fails the final link with -s
    # ("Invalid operation").
    ldflags=-static
    strip=ppc-amigaos-strip
    sqlite_cflags=
    ;;
  aros-x86)
    # This gcc has no include path of its own. Under -std=c89 (__STRICT_ANSI__)
    # AROS's headers hide the POSIX layer the file code needs; _XOPEN_SOURCE
    # restores what they give by default, and inline/restrict/asm are spelled
    # as GNU C89 spells them for the posixc and proto/dos.h inline headers.
    cc="x86_64-aros-gcc --sysroot=/opt/x86_64-aros"
    cflags="-D_XOPEN_SOURCE=500 -Dinline=__inline__ -Drestrict=__restrict__ -Dasm=__asm__"
    # Not stripped: an AROS executable is a relocatable object, and the
    # loader needs its relocation symbols.
    ldflags=
    strip=
    sqlite_cflags=-D_XOPEN_SOURCE=500
    ;;
  amigaos-m68k)
    # 68040 with its FPU, libnix (-mcrt=nix20, AmigaOS 2.0+ C library), as
    # the image's CMake toolchain file builds SDL2. libnix's headers declare
    # static inline functions; __inline__ is how C89 spells it.
    cc="m68k-amigaos-gcc -mcrt=nix20 -m68040 -mhard-float"
    cflags=-Dinline=__inline__
    # libnix grows the stack to __stack (client/desktop.c) only when its
    # swapstack module is linked, and nothing else refers to it.
    ldflags="-s -Wl,-u,___stkswap"
    strip=
    sqlite_cflags=
    ;;
  *)
    echo "unknown Amiga desktop target: $target" >&2
    exit 2
    ;;
esac
mkdir -p "$work" "$(dirname -- "$output")"
TMPDIR="$work/tmp"
export TMPDIR
mkdir -p "$TMPDIR"

# SDL2, static, with only what Wena uses where we build it ourselves.
case "$target" in
  amigaos4-ppc)
    # The image's SDL2 2.30.5, built for newlib; its static archive.
    sdl_cflags="-isystem /opt/ppc-amigaos/usr/include/SDL2"
    sdl_libs=/opt/ppc-amigaos/usr/lib/libSDL2.a
    test -f "$sdl_libs"
    ;;
  aros-x86)
    if [ ! -f "$work/sdl/libSDL2.a" ]; then
      rm -rf "$work/sdl-src" "$work/sdl"
      mkdir -p "$work/sdl-src" "$work/sdl/obj"
      tar -xzf "$(fetch sdl2)" -C "$work/sdl-src" --strip-components=1
      (cd "$work/sdl-src" && patch -p1 -s < "$(fetch sdl2-aros-patch)")
      cp "$(fetch sdl2-aros-static)" "$work/sdl-src/SDL2_static.c"
      cp "$(fetch sdl2-aros-intern)" "$work/sdl-src/SDL2_intern.h"
      # No OpenGL: Wena draws with SDL's software renderer, and AROS's
      # libGL.a would tie the executable to gl.library. The driver's GL hooks
      # stay NULL, so SDL reports OpenGL as unsupported. This image has no
      # libiconv and no wcslen/wcscmp; SDL has its own of each.
      sed -i -e '/#define SDL_VIDEO_RENDER_OGL /d' -e '/#define SDL_VIDEO_OPENGL /d' \
        -e '/#define SDL_VIDEO_OPENGL_AGL /d' -e '/#define HAVE_ICONV /d' \
        -e '/#define HAVE_ICONV_H /d' -e '/#define SDL_USE_LIBICONV /d' \
        -e '/#define HAVE_WCSLEN /d' -e '/#define HAVE_WCSCMP /d' \
        "$work/sdl-src/include/SDL_config_aros.h"
      sed -i -e '/device->GL_[A-Za-z]* = AROS_GL_/d' "$work/sdl-src/src/video/aros/SDL_arosvideo.c"
      # The static library SDL2_static of AROS's SDL2/main/mmakefile.src:
      # its FILES, SDL2AROSCOREFILES and SDL2AROSHWFILES, less render/opengl
      # and SDL_arosopengl, plus SDL2_static.c (-DSDL2_AROS_STATIC).
      # ADATE is the build date the About requester shows; the AROS port's
      # commit date keeps the build reproducible.
      sdl_files="atomic/SDL_atomic atomic/SDL_spinlock audio/SDL_audio audio/SDL_audiocvt
        audio/SDL_audiodev audio/SDL_audiotypecvt audio/SDL_mixer audio/SDL_wave cpuinfo/SDL_cpuinfo
        events/imKStoUCS events/SDL_clipboardevents events/SDL_displayevents events/SDL_dropevents
        events/SDL_events events/SDL_gesture events/SDL_keyboard events/SDL_keysym_to_scancode
        events/SDL_mouse events/SDL_quit events/SDL_scancode_tables events/SDL_touch
        events/SDL_windowevents file/SDL_rwops haptic/SDL_haptic hidapi/SDL_hidapi
        joystick/controller_type joystick/SDL_gamecontroller joystick/SDL_joystick
        joystick/SDL_steam_virtual_gamepad locale/SDL_locale misc/SDL_url power/SDL_power
        render/SDL_d3dmath render/SDL_render render/SDL_yuv_sw render/software/SDL_blendfillrect
        render/software/SDL_blendline render/software/SDL_blendpoint render/software/SDL_drawline
        render/software/SDL_drawpoint render/software/SDL_render_sw render/software/SDL_rotate
        render/software/SDL_triangle SDL_assert SDL_dataqueue SDL_error SDL_guid SDL_hints SDL_list
        SDL_log SDL_utils sensor/SDL_sensor stdlib/SDL_malloc stdlib/SDL_crc16 stdlib/SDL_crc32
        stdlib/SDL_iconv stdlib/SDL_mslibc stdlib/SDL_qsort stdlib/SDL_stdlib stdlib/SDL_string
        stdlib/SDL_strtokr thread/generic/SDL_syscond thread/generic/SDL_systls thread/SDL_thread
        timer/SDL_timer video/dummy/SDL_nullevents video/dummy/SDL_nullframebuffer
        video/dummy/SDL_nullvideo video/SDL_blit video/SDL_blit_0 video/SDL_blit_1 video/SDL_blit_A
        video/SDL_blit_auto video/SDL_blit_copy video/SDL_blit_N video/SDL_blit_slow video/SDL_bmp
        video/SDL_clipboard video/SDL_egl video/SDL_fillrect video/SDL_pixels video/SDL_rect
        video/SDL_RLEaccel video/SDL_shape video/SDL_stretch video/SDL_surface video/SDL_video
        video/SDL_vulkan_utils video/SDL_yuv video/yuv2rgb/yuv_rgb_lsx video/yuv2rgb/yuv_rgb_sse
        video/yuv2rgb/yuv_rgb_std SDL
        core/aros/SDL_cpu filesystem/aros/SDL_sysfilesystem locale/aros/SDL_syslocale
        misc/amigaos/SDL_sysurl misc/aros/SDL_getenv misc/aros/SDL_misc thread/aros/SDL_sysmutex
        thread/aros/SDL_syssem thread/aros/SDL_systhread timer/aros/SDL_systimer
        video/aros/SDL_arosclipboard video/aros/SDL_arosevents video/aros/SDL_arosframebuffer
        video/aros/SDL_aroskeyboard video/aros/SDL_arosmessagebox video/aros/SDL_arosmodes
        video/aros/SDL_arosmouse video/aros/SDL_arosshape video/aros/SDL_arosvideo
        video/aros/SDL_aroswindow
        audio/dummy/SDL_dummyaudio audio/ahi/SDL_ahi_audio haptic/dummy/SDL_syshaptic
        joystick/aros/SDL_sysjoystick power/aros/SDL_syspower sensor/dummy/SDL_dummysensor"
      for file in SDL2_static $(printf 'src/%s ' $sdl_files); do
        $cc -std=gnu99 -O2 -DSDL2_AROS_STATIC -D_GNU_SOURCE=1 -DADATE=\"13.09.2026\" \
          -I"$work/sdl-src/include" -I"$work/sdl-src" -I"$work/sdl-src/src" \
          -c "$work/sdl-src/$file.c" -o "$work/sdl/obj/$(echo "$file" | tr / _).o"
      done
      x86_64-aros-ar rcs "$work/sdl/libSDL2.a.part" "$work"/sdl/obj/*.o
      mv "$work/sdl/libSDL2.a.part" "$work/sdl/libSDL2.a"
    fi
    sdl_cflags="-isystem $work/sdl-src/include"
    sdl_libs="$work/sdl/libSDL2.a"
    ;;
  amigaos-m68k)
    if [ ! -f "$work/sdl/lib/libSDL2.a" ]; then
      rm -rf "$work/sdl-src" "$work/sdl-build" "$work/sdl"
      mkdir -p "$work/sdl-src"
      tar -xzf "$(fetch sdl2-amigaos3)" -C "$work/sdl-src" --strip-components=1
      # DevilutionX's AmigaOS 3 options (68040, hard FPU, -fbbb=- turns off
      # the m68k-specific optimiser pass), static, RTG only (no AGA
      # chunky-to-planar), and none of the subsystems Wena does not use.
      cmake -S "$work/sdl-src" -B "$work/sdl-build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$work/sdl" -DM68K_CPU=68040 -DM68K_FPU=hard \
        "-DM68K_COMMON=-s -fbbb=- -ffast-math" -DSDL_SHARED=OFF -DSDL_STATIC=ON \
        -DSDL_TEST=OFF -DSDL_TESTS=OFF -DSDL_AMIGAOS3_LIBRARY=OFF -DSDL_AMIGAOS3_AGA=OFF \
        -DSDL_AUDIO=OFF -DSDL_JOYSTICK=OFF -DSDL_HAPTIC=OFF -DSDL_HIDAPI=OFF \
        -DSDL_SENSOR=OFF -DSDL_POWER=OFF > "$work/sdl-configure.log" 2>&1 ||
        { tail -40 "$work/sdl-configure.log" >&2; exit 1; }
      cmake --build "$work/sdl-build" -j "$jobs" > "$work/sdl-make.log" 2>&1 ||
        { tail -40 "$work/sdl-make.log" >&2; exit 1; }
      cmake --build "$work/sdl-build" --target install > "$work/sdl-install.log" 2>&1 ||
        { tail -40 "$work/sdl-install.log" >&2; exit 1; }
    fi
    sdl_cflags="-isystem $work/sdl/include/SDL2"
    sdl_libs="$(sh "$work/sdl/bin/sdl2-config" --static-libs)"
    ;;
esac

# SQLite, one object from the amalgamation, plus server/sqlite_amiga_vfs.c.
#   THREADSAFE=0          single-threaded desktop; libnix has no threads.
#   OMIT_LOAD_EXTENSION   no dlopen() on these systems.
#   DQS=0                 as every other release build.
#   OMIT_WAL              WAL needs shared memory and fcntl() locks, which
#                         these C libraries do not have; Wena's
#                         "PRAGMA journal_mode=WAL" then keeps the rollback
#                         journal (DELETE), and wal_checkpoint is a no-op.
#   MAX_MMAP_SIZE=0       no mmap().
#   TEMP_STORE=3, STMTJRNL_SPILL=-1
#                         temporary tables, sorts and statement journals in
#                         memory: the unix VFS would put temporary files in
#                         /var/tmp, /tmp or ".", none of them AmigaDOS paths.
#   DISABLE_DIRSYNC       AmigaDOS has no directory fsync().
#   HAVE_NANOSLEEP=0      not declared by every one of these C libraries;
#                         sleep() instead (no locks, so SQLite never waits).
#   fchmod, fchown        no-ops in sqlite_amiga_vfs.c: AmigaDOS has no
#                         Unix owners or modes (and libnix no such calls).
#   EXTRA_INIT            registers the "amiga" VFS (unix-none, AmigaDOS
#                         names) as the default, see sqlite_amiga_vfs.c.
sqlite_dir="$work/sqlite"
rm -rf "$sqlite_dir"
mkdir -p "$sqlite_dir"
python3 - "$(fetch sqlite)" "$sqlite_dir" <<'PY'
import sys, zipfile
from pathlib import Path
archive, target = sys.argv[1], Path(sys.argv[2])
with zipfile.ZipFile(archive) as bundle:
    for name in bundle.namelist():
        base = name.rsplit("/", 1)[-1]
        if base in {"sqlite3.c", "sqlite3.h"}:
            (target / base).write_bytes(bundle.read(name))
PY
$cc -O2 $sqlite_cflags -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_DQS=0 \
  -DSQLITE_OMIT_WAL -DSQLITE_MAX_MMAP_SIZE=0 -DSQLITE_TEMP_STORE=3 -DSQLITE_STMTJRNL_SPILL=-1 \
  -DSQLITE_DISABLE_DIRSYNC -DHAVE_NANOSLEEP=0 -Dfchmod=wena_sqlite_fchmod \
  -Dfchown=wena_sqlite_fchown -DSQLITE_EXTRA_INIT=wena_sqlite_amiga_init \
  -c "$sqlite_dir/sqlite3.c" -o "$sqlite_dir/sqlite3.o"
$cc -std=c89 -pedantic-errors -Wall -Wextra -Werror -O2 $cflags -isystem "$sqlite_dir" \
  -c "$root_dir/server/sqlite_amiga_vfs.c" -o "$sqlite_dir/sqlite_amiga_vfs.o"

WENA_CC=$cc \
WENA_SDL_CFLAGS=$sdl_cflags \
WENA_SDL_LIBS=$sdl_libs \
WENA_SQLITE_CFLAGS="-isystem $sqlite_dir" \
WENA_SQLITE_LIBS="$sqlite_dir/sqlite3.o $sqlite_dir/sqlite_amiga_vfs.o" \
WENA_LDFLAGS=$ldflags \
WENA_CFLAGS="-O2 $cflags" \
  sh "$root_dir/scripts/build_desktop.sh" "$output"
if [ -n "$strip" ]; then "$strip" "$output"; fi

# Prove it: the executable format each system loads, and no shared library.
magic=$(od -An -tx1 -N6 "$output" | tr -d ' \n')
case "$target" in
  amigaos-m68k)
    # HUNK_HEADER: AmigaOS loadseg()ble executable.
    test "${magic%????}" = 000003f3 || { echo "$output: not a HUNK executable ($magic)" >&2; exit 1; }
    ;;
  amigaos4-ppc)
    # ELF, 32-bit, big-endian, PowerPC executable, no dynamic section.
    test "$magic" = 7f454c460102 || { echo "$output: not ELF32 big-endian ($magic)" >&2; exit 1; }
    ppc-amigaos-readelf -h "$output" | grep -Eq 'Machine:[[:space:]]+PowerPC$'
    ppc-amigaos-readelf -h "$output" | grep -Eq 'Type:[[:space:]]+EXEC'
    if ppc-amigaos-readelf -d "$output" | grep -q NEEDED; then
      echo "$output: needs shared objects" >&2; ppc-amigaos-readelf -d "$output" >&2; exit 1
    fi
    ;;
  aros-x86)
    # ELF, 64-bit, little-endian, x86-64 relocatable object, which is what
    # AROS loads; nothing is resolved at run time but the OS's libraries.
    test "$magic" = 7f454c460201 || { echo "$output: not ELF64 little-endian ($magic)" >&2; exit 1; }
    x86_64-aros-readelf -h "$output" | grep -Eq 'Machine:[[:space:]]+Advanced Micro Devices X86-64'
    x86_64-aros-readelf -h "$output" | grep -Eq 'Type:[[:space:]]+REL'
    if x86_64-aros-readelf -d "$output" 2>/dev/null | grep -q NEEDED; then
      echo "$output: needs shared objects" >&2; exit 1
    fi
    if x86_64-aros-nm -u "$output" | grep -Eiq 'SDL_|sqlite3_'; then
      echo "$output: unresolved SDL or SQLite symbols" >&2; exit 1
    fi
    ;;
esac
rm -rf "$TMPDIR"
echo "$output"
