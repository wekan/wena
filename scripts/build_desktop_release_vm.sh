#!/bin/sh
# Inside a virtual machine of a BSD or Haiku (cross-platform-actions in
# .github/workflows/release-all.yml): install what the build needs with the
# system's own package manager, build the self-contained desktop, smoke-test
# it headless, and write its release file to release/.
#
#   sh scripts/build_desktop_release_vm.sh freebsd-amd64
#
# SDL2 and SQLite are built from the pinned sources as on every platform. The
# windowing system is the system's own X11 (Haiku: its app_server), which SDL
# loads when it starts; on NetBSD it is linked, because /usr/X11R7/lib is not
# where NetBSD's dynamic linker looks for a library opened by name.
set -eu
target=$1
as_root() { if test "$(id -u)" -eq 0; then "$@"; else sudo "$@"; fi; }
case "$target" in
  freebsd-*|dragonflybsd-*)
    as_root env ASSUME_ALWAYS_YES=yes pkg install -y gmake python3 pkgconf xorgproto libX11 libXext \
      libXrandr libXcursor libXi libXfixes libXScrnSaver libxkbcommon > /dev/null
    export MAKE=gmake
    ;;
  netbsd-*)
    as_root pkgin -y install gmake python313 pkgconf > /dev/null
    test -e /usr/pkg/bin/python3 || as_root ln -s python3.13 /usr/pkg/bin/python3
    export MAKE=gmake
    export WENA_SDL_CONFIGURE="--x-includes=/usr/X11R7/include --x-libraries=/usr/X11R7/lib --disable-x11-shared"
    export WENA_EXTRA_LDFLAGS="-Wl,-R/usr/X11R7/lib"
    ;;
  openbsd-*)
    as_root pkg_add -I gmake python%3 > /dev/null
    export MAKE=gmake
    export WENA_SDL_CONFIGURE="--x-includes=/usr/X11R6/include --x-libraries=/usr/X11R6/lib"
    ;;
  haiku-*)
    pkgman install -y make python3.10 pkgconfig > /dev/null
    command -v python3 > /dev/null || ln -s "$(command -v python3.10)" /boot/home/config/non-packaged/bin/python3
    ;;
  *)
    echo "not a virtual-machine target: $target" >&2
    exit 2
    ;;
esac
mkdir -p .tools/tmp
TMPDIR=$PWD/.tools/tmp
export TMPDIR
executable="dist/release/$target/wena"
sh scripts/build_desktop_release.sh "$target" "$executable"
smoke=$(mktemp -d)
for run in 1 2; do
  WENA_DATABASE="$smoke/board.sqlite" WENA_LOG_DIR="$smoke/log" SDL_VIDEODRIVER=dummy "$executable" --smoke
done
cat "$smoke/log/desktop.log"
"$executable" --licenses > /dev/null
python3 scripts/package_desktop_release.py binary "$target" "$executable" release
