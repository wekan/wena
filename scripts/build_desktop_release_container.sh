#!/bin/sh
# Inside a Debian or Ubuntu container of the target CPU (native, or under
# QEMU): install what the build needs, build the self-contained desktop,
# smoke-test it headless and against a real X server, and write its release
# file to release/. Used by .github/workflows/release-all.yml:
#
#   docker run --rm --platform linux/riscv64 -v "$PWD:/w" -w /w ubuntu:22.04 \
#     sh scripts/build_desktop_release_container.sh linux-riscv64
set -eu
target=$1
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq --no-install-recommends build-essential python3 pkg-config file \
  ca-certificates xz-utils xvfb xauth \
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxfixes-dev libxss-dev \
  libxkbcommon-dev libwayland-dev wayland-protocols libegl-dev libgl-dev libdrm-dev libgbm-dev \
  > /dev/null
mkdir -p .tools/tmp
TMPDIR=$PWD/.tools/tmp
export TMPDIR
executable="dist/release/$target/wena"
sh scripts/build_desktop_release.sh "$target" "$executable"
smoke=$(mktemp -d)
WENA_DATABASE="$smoke/board.sqlite" WENA_LOG_DIR="$smoke/log" SDL_VIDEODRIVER=dummy "$executable" --smoke
# Under QEMU, loading Mesa's DRI driver (it links LLVM) crashes on MIPS64 with
# SIGBUS before Wena draws anything, whichever Gallium driver is chosen. There
# the X11 run keeps Mesa's driver unloaded and uses SDL's software renderer.
if test "$target" = linux-mips64le; then
  export LIBGL_DRIVERS_PATH=/nonexistent SDL_RENDER_DRIVER=software
fi
# xvfb-run's status is the X server's as well; the app's own is the one that counts.
WENA_DATABASE="$smoke/board.sqlite" WENA_LOG_DIR="$smoke/log" \
  xvfb-run -a sh -c '"$0" --smoke; echo "$?" > "$1"' "$executable" "$smoke/status" || true
test "$(cat "$smoke/status")" = 0
cat "$smoke/log/desktop.log"
python3 scripts/package_desktop_release.py binary "$target" "$executable" release
# A local build (scripts/toolchain.py) gives its user the files it wrote here as root.
if test -n "${WENA_OWNER:-}"; then chown -R "$WENA_OWNER" dist release .tools; fi
