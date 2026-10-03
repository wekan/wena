#!/usr/bin/env sh
# One self-contained desktop executable for a release target:
#
#   scripts/build_desktop_release.sh TARGET OUTPUT_EXECUTABLE
#
# SDL2 and SQLite are built from the pinned, checksum-verified sources in
# config/release-dependencies.json and linked statically, so the executable
# needs only what every system of that kind already has: the C library and,
# on Linux and the BSDs, the windowing system that SDL loads when it starts.
# SDL is built with only what Wena uses (video, rendering, events, clipboard,
# cursors, timers): no audio, joysticks, haptics, sensors, HID or power.
#
# Linux targets build natively (a matching runner, or a container under QEMU
# for the other CPUs); macOS targets with clang -arch on a macOS runner; the
# Windows targets cross-compile with MinGW-w64. CC overrides the compiler.
# Symbols are stripped at link time (-s): the catalog and migrations are
# appended to the executable afterwards, and a later `strip` would drop them.
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if [ "$#" -ne 2 ]; then
  echo "Usage: scripts/build_desktop_release.sh TARGET OUTPUT_EXECUTABLE" >&2
  exit 2
fi
target=$1
output=$2
work="$root_dir/.tools/release/$target"
cache=${WENA_RELEASE_CACHE:-$root_dir/.tools/cache}
jobs=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)

host=
ldflags=
cflags=
# Windows 7 and later (the UTF-8 and locale APIs Wena uses). Nuklear converts
# pointers through __PTRDIFF_TYPE__, which is long long on 64-bit Windows, so
# C89's long long warning is off for the Windows builds only.
windows_cflags="-D_WIN32_WINNT=0x0601 -DWINVER=0x0601 -Wno-long-long"
sdl_configure=
case "$target" in
  linux-*|freebsd-*)
    cc=${CC:-cc}
    ldflags="-pthread -s"
    ;;
  macos-arm64)
    cc=${CC:-"clang -arch arm64 -mmacosx-version-min=11.0"}
    host=aarch64-apple-darwin
    ;;
  macos-amd64)
    cc=${CC:-"clang -arch x86_64 -mmacosx-version-min=10.13"}
    host=x86_64-apple-darwin
    ;;
  windows-amd64)
    cc=${CC:-x86_64-w64-mingw32-gcc}
    cflags=$windows_cflags
    host=x86_64-w64-mingw32
    ldflags="-static -static-libgcc -s"
    ;;
  windows-i686)
    cc=${CC:-i686-w64-mingw32-gcc}
    cflags=$windows_cflags
    host=i686-w64-mingw32
    ldflags="-static -static-libgcc -s"
    ;;
  windows-arm64)
    cc=${CC:-aarch64-w64-mingw32-clang}
    cflags=$windows_cflags
    host=aarch64-w64-mingw32
    ldflags="-static -s"
    ;;
  *)
    echo "unknown desktop release target: $target" >&2
    exit 2
    ;;
esac

mkdir -p "$work" "$(dirname -- "$output")"
# Windows arm64 needs Clang's MinGW-w64: the pinned llvm-mingw when none is installed.
if [ "$target" = windows-arm64 ] && ! command -v aarch64-w64-mingw32-clang >/dev/null 2>&1; then
  case $(uname -m) in aarch64|arm64) toolchain=llvm-mingw-aarch64 ;; *) toolchain=llvm-mingw-x86_64 ;; esac
  if [ ! -x "$root_dir/.tools/release/$toolchain/bin/aarch64-w64-mingw32-clang" ]; then
    archive=$(python3 "$root_dir/scripts/fetch_release_dependency.py" "$toolchain" "$cache")
    rm -rf "$root_dir/.tools/release/$toolchain"
    mkdir -p "$root_dir/.tools/release/$toolchain"
    tar -xJf "$archive" -C "$root_dir/.tools/release/$toolchain" --strip-components=1
  fi
  PATH="$root_dir/.tools/release/$toolchain/bin:$PATH"
  export PATH
fi
sdl_archive=$(python3 "$root_dir/scripts/fetch_release_dependency.py" sdl2 "$cache")
sqlite_archive=$(python3 "$root_dir/scripts/fetch_release_dependency.py" sqlite "$cache")

# SDL2, static and minimal, installed into $work/sdl.
if [ ! -f "$work/sdl/lib/libSDL2.a" ]; then
  rm -rf "$work/sdl-src" "$work/sdl"
  mkdir -p "$work/sdl-src"
  tar -xzf "$sdl_archive" -C "$work/sdl-src" --strip-components=1
  (
    cd "$work/sdl-src"
    ./configure ${host:+--host="$host"} CC="$cc" --prefix="$work/sdl" \
      --disable-shared --enable-static \
      --disable-audio --disable-joystick --disable-haptic --disable-hidapi \
      --disable-sensor --disable-power $sdl_configure > "$work/sdl-configure.log" 2>&1 ||
      { tail -40 "$work/sdl-configure.log" >&2; exit 1; }
    make -j"$jobs" > "$work/sdl-make.log" 2>&1 || { tail -40 "$work/sdl-make.log" >&2; exit 1; }
    make install > "$work/sdl-install.log" 2>&1
  )
fi

# SQLite, one object from the amalgamation.
sqlite_dir="$work/sqlite"
rm -rf "$sqlite_dir"
mkdir -p "$sqlite_dir"
python3 - "$sqlite_archive" "$sqlite_dir" <<'PY'
import sys, zipfile
from pathlib import Path
archive, target = sys.argv[1], Path(sys.argv[2])
with zipfile.ZipFile(archive) as bundle:
    for name in bundle.namelist():
        base = name.rsplit("/", 1)[-1]
        if base in {"sqlite3.c", "sqlite3.h"}:
            (target / base).write_bytes(bundle.read(name))
PY
$cc -O2 -DSQLITE_THREADSAFE=1 -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_DQS=0 \
  -c "$sqlite_dir/sqlite3.c" -o "$sqlite_dir/sqlite3.o"

sdl_config="$work/sdl/bin/sdl2-config"
WENA_CC=$cc \
WENA_SDL_CFLAGS="$("$sdl_config" --cflags)" \
WENA_SDL_LIBS="$("$sdl_config" --static-libs)" \
WENA_SQLITE_CFLAGS="-isystem $sqlite_dir" \
WENA_SQLITE_LIBS="$sqlite_dir/sqlite3.o" \
WENA_LDFLAGS=$ldflags \
WENA_CFLAGS=$cflags \
  sh "$root_dir/scripts/build_desktop.sh" "$output"

# Prove it: no SDL2 or SQLite shared library is loaded at run time.
python3 "$root_dir/scripts/check_release_executable.py" "$target" "$output"
