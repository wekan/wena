#!/usr/bin/env sh
# Wena's desktop as an iOS app (arm64), on macOS with Xcode:
#
#   scripts/build_desktop_ios.sh OUTPUT_IPA                 iPhone and iPad, unsigned .ipa
#   scripts/build_desktop_ios.sh --simulator OUTPUT_APP     Wena.app for the iOS Simulator
#
# SDL2 is built for iOS from the pinned SDL2 source (config/release-dependencies.json)
# with its CMake build, static and with only what Wena uses (video, rendering,
# events), and SQLite from the pinned amalgamation. client/desktop.c and the
# rest of scripts/build_desktop.sh's sources are linked with them and SDL's
# UIKit main into Wena.app/Wena, which loads nothing but iOS's own frameworks.
# CMake rather than SDL's Xcode project: that project compiles SDL's Metal
# shaders, which needs the Metal Toolchain, a separate Xcode download since
# Xcode 26; CMake uses the shaders SDL ships precompiled.
#
# THE .ipa IS NOT SIGNED. iOS installs only signed apps: re-sign it with your
# own certificate and provisioning profile, or with AltStore or Sideloadly.
# The bundle id is fi.wekan.wena. The Simulator app is ad-hoc signed by the
# linker, which is all the Simulator needs.
#
# Needs: macOS, Xcode with the iOS SDKs (xcrun; DEVELOPER_DIR picks an Xcode
# other than the selected one) and cmake. WENA_VERSION (vMAJOR.MINOR) names
# the version; else the next one from CHANGELOG.md.
#
# Smoke test in the Simulator, which takes --smoke as a launch argument:
#   xcrun simctl install booted OUTPUT_APP
#   xcrun simctl launch --console booted fi.wekan.wena --smoke
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
platform=iphoneos
if [ "${1:-}" = --simulator ]; then
  platform=iphonesimulator
  shift
fi
if [ "$#" -ne 1 ]; then
  echo "Usage: scripts/build_desktop_ios.sh [--simulator] OUTPUT_IPA_OR_APP" >&2
  exit 2
fi
case $1 in
  /*) output=$1 ;;
  *) output=$(pwd)/$1 ;;
esac

# The oldest iOS that Xcode 16 (the release runner's) and Xcode 26 and later
# all still build for.
minimum_os=15.0
fail() {
  echo "build_desktop_ios: $*" >&2
  exit 1
}

[ "$(uname -s)" = Darwin ] || fail "iOS builds only on macOS with Xcode"
command -v xcrun >/dev/null 2>&1 || fail "xcrun not found: install Xcode (set DEVELOPER_DIR when it is not the selected one)"
sdk=$(xcrun --sdk "$platform" --show-sdk-path 2>/dev/null) ||
  fail "no $platform SDK: install Xcode with iOS (set DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer when the Command Line Tools are selected)"
sdk_version=$(xcrun --sdk "$platform" --show-sdk-version)
clang=$(xcrun --sdk "$platform" --find clang)
command -v cmake >/dev/null 2>&1 || fail "cmake not found (brew install cmake)"
if [ "$platform" = iphoneos ]; then
  target=arm64-apple-ios$minimum_os
  plist_platform=iPhoneOS
  # Unsigned: not even the linker's ad-hoc signature, which re-signing replaces anyway.
  sign_flags=-Wl,-no_adhoc_codesign
else
  target=arm64-apple-ios$minimum_os-simulator
  plist_platform=iPhoneSimulator
  sign_flags=
fi
cc="$clang -isysroot $sdk -target $target"

version=${WENA_VERSION:-$(python3 "$root_dir/scripts/release_version.py" next)}
case $version in
  v[0-9]*.[0-9][0-9]) ;;
  *) fail "WENA_VERSION must look like v0.01 (got $version)" ;;
esac

work=$root_dir/.tools/release/ios-arm64-$platform
cache=${WENA_RELEASE_CACHE:-$root_dir/.tools/cache}
jobs=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)
mkdir -p "$work" "$(dirname -- "$output")"
sdl_archive=$(python3 "$root_dir/scripts/fetch_release_dependency.py" sdl2 "$cache")
sqlite_archive=$(python3 "$root_dir/scripts/fetch_release_dependency.py" sqlite "$cache")

# --- SDL2, static and minimal, installed into $work/sdl.
if [ ! -f "$work/sdl/lib/libSDL2.a" ] || [ ! -f "$work/sdl/lib/libSDL2main.a" ]; then
  rm -rf "$work/sdl-src" "$work/sdl-build" "$work/sdl"
  mkdir -p "$work/sdl-src"
  tar -xzf "$sdl_archive" -C "$work/sdl-src" --strip-components=1
  {
    cmake -S "$work/sdl-src" -B "$work/sdl-build" -DCMAKE_SYSTEM_NAME=iOS \
      -DCMAKE_OSX_SYSROOT="$platform" -DCMAKE_OSX_ARCHITECTURES=arm64 \
      -DCMAKE_OSX_DEPLOYMENT_TARGET="$minimum_os" -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="$work/sdl" -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST=OFF \
      -DSDL_AUDIO=OFF -DSDL_JOYSTICK=OFF -DSDL_HAPTIC=OFF -DSDL_HIDAPI=OFF \
      -DSDL_SENSOR=OFF -DSDL_POWER=OFF &&
    cmake --build "$work/sdl-build" -j "$jobs" &&
    cmake --install "$work/sdl-build"
  } > "$work/sdl-build.log" 2>&1 ||
    { tail -40 "$work/sdl-build.log" >&2; fail "SDL2 did not build (log: $work/sdl-build.log)"; }
fi

# --- SQLite, one object from the amalgamation.
sqlite_dir=$work/sqlite
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
# No proxy locking (gethostuuid is not available on iOS).
$cc -O2 -DSQLITE_THREADSAFE=1 -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_DQS=0 -DSQLITE_ENABLE_LOCKING_STYLE=0 -DHAVE_GETHOSTUUID=0 \
  -c "$sqlite_dir/sqlite3.c" -o "$sqlite_dir/sqlite3.o"

# --- Wena.app/Wena: build_desktop.sh's sources and flags, with SDL's UIKit
# main (libSDL2main), which starts the app and calls SDL_main. GameController
# is not in sdl2-config's list without the joystick, but SDL's UIKit events use
# it for an iPad's hardware keyboard and mouse.
# The UIScene glue (client/platform/ios/scene.m), the one Objective-C file.
$cc -O2 -fobjc-arc -Wall -Wextra -Werror $("$work/sdl/bin/sdl2-config" --cflags) \
  -c "$root_dir/client/platform/ios/scene.m" -o "$work/scene.o"
app=$work/Payload/Wena.app
rm -rf "$work/Payload"
mkdir -p "$app"
sdl_config=$work/sdl/bin/sdl2-config
WENA_CC=$cc \
WENA_CFLAGS=-O2 \
WENA_SDL_CFLAGS="$("$sdl_config" --cflags)" \
WENA_SDL_LIBS="$work/scene.o $work/sdl/lib/libSDL2main.a $("$sdl_config" --static-libs) -Wl,-framework,GameController" \
WENA_SQLITE_CFLAGS="-isystem $sqlite_dir" \
WENA_SQLITE_LIBS="$sqlite_dir/sqlite3.o" \
WENA_LDFLAGS="-Wl,-dead_strip -Wl,-S -Wl,-x $sign_flags" \
  sh "$root_dir/scripts/build_desktop.sh" "$app/Wena"

# --- The bundle: Info.plist, PkgInfo and the icons.
sed -e "s/@VERSION@/${version#v}/g" -e "s/@PLATFORM@/$plist_platform/g" \
    -e "s/@PLATFORM_NAME@/$platform/g" -e "s/@SDK_VERSION@/$sdk_version/g" \
    -e "s/@MINIMUM_OS@/$minimum_os/g" \
    "$root_dir/client/platform/ios/Info.plist" > "$app/Info.plist"
plutil -lint "$app/Info.plist" >/dev/null || fail "Info.plist is not valid"
if grep -Eq '@(VERSION|PLATFORM|PLATFORM_NAME|SDK_VERSION|MINIMUM_OS)@' "$app/Info.plist"; then
  fail "Info.plist has an unfilled @NAME@"
fi
printf 'APPL????' > "$app/PkgInfo"
python3 "$root_dir/scripts/mobile_icon.py" 120 "$app/AppIcon60x60@2x.png"
python3 "$root_dir/scripts/mobile_icon.py" 180 "$app/AppIcon60x60@3x.png"
python3 "$root_dir/scripts/mobile_icon.py" 152 "$app/AppIcon76x76@2x~ipad.png"

# Prove it: arm64 for this platform, loading only iOS's own libraries.
binary=$app/Wena
file "$binary" | grep -q 'Mach-O 64-bit executable arm64' || fail "Wena is not an arm64 executable"
test "$(xcrun lipo -archs "$binary")" = arm64 || fail "Wena is not arm64 only"
if [ "$platform" = iphoneos ]; then build_platform=IOS; else build_platform=IOSSIMULATOR; fi
xcrun vtool -show-build "$binary" | grep -Eq "platform[[:space:]]+$build_platform\$" ||
  fail "Wena is not built for $build_platform"
xcrun otool -L "$binary" | sed 1d | while read -r library _; do
  case $library in
    /System/Library/Frameworks/*|/usr/lib/libSystem.B.dylib|/usr/lib/libobjc.A.dylib|/usr/lib/libc++.1.dylib|/usr/lib/libiconv.2.dylib) ;;
    *) fail "Wena loads $library, which is not part of iOS" ;;
  esac
done

if [ "$platform" = iphonesimulator ]; then
  rm -rf "$output"
  cp -R "$app" "$output"
  echo "iOS Simulator app: $output (fi.wekan.wena ${version#v}, iOS $minimum_os or later)"
  exit 0
fi
rm -f "$output"
(cd "$work" && ditto -c -k --norsrc --keepParent Payload "$output")
cat >&2 <<EOF
NOTE: $output is NOT signed. iOS installs only signed apps: re-sign it with
your own certificate and provisioning profile, or with AltStore or Sideloadly.
EOF
echo "iOS IPA: $output (fi.wekan.wena ${version#v}, iOS $minimum_os or later, unsigned)"
