#!/usr/bin/env sh
# Wena's desktop as an installable Android app (arm64-v8a):
#
#   scripts/build_desktop_android.sh OUTPUT_APK
#
# The APK is SDL2's own Android glue - the org.libsdl.app Java classes from the
# pinned SDL2 source (config/release-dependencies.json) - with a small
# fi.wekan.wena.WenaActivity, and lib/arm64-v8a/libmain.so: client/desktop.c
# and the rest of scripts/build_desktop.sh's sources, with SDL2 (ndk-build of
# SDL's own Android.mk) and SQLite (the pinned amalgamation) linked in. Nothing
# else is loaded at run time but Android's own libraries.
#
# Built directly, without Gradle: NDK clang for the library, javac and d8 for
# the classes, aapt2 for the manifest and resources, zipalign and apksigner.
# Gradle would add a wrapper download, the Android Gradle plugin and its Maven
# dependencies - each a version to pin and a network fetch - to package what
# is six files. Every tool here is in the pinned NDK, the pinned SDK packages
# or the JDK.
#
# What it needs, and where it looks:
#   ANDROID_NDK_ROOT  NDK r29 (29.0.14206865); else ANDROID_NDK_HOME, else
#                     $ANDROID_HOME/ndk/29.0.14206865, else .tools/android-ndk-r29
#                     (./build.sh installs that one).
#   ANDROID_HOME      the Android SDK (else ANDROID_SDK_ROOT, else .tools/android-sdk)
#                     with platforms;android-37.0 and build-tools;37.0.0. When they
#                     are missing they are installed with its
#                     cmdline-tools/latest/bin/sdkmanager; when that is missing too
#                     and ANDROID_HOME is .tools/android-sdk, the pinned
#                     cmdline-tools are downloaded there first.
#   JAVA_HOME         a JDK 17 or newer (javac, java, keytool); else the one on PATH.
#   .tools is the folder this checkout is in when that is called .tools, else
#   the one inside it (as scripts/toolchain.py).
#
# Signing - an APK installs only when signed:
#   WENA_ANDROID_KEYSTORE, WENA_ANDROID_KEYSTORE_PASSWORD, WENA_ANDROID_KEY_ALIAS
#   (and WENA_ANDROID_KEY_PASSWORD, when the key's password differs) sign the
#   release. Without them a debug key is made in the build folder, with a
#   warning: an APK signed with another key does not install over it.
# WENA_VERSION (vMAJOR.MINOR) names the version; else the next one from
# CHANGELOG.md (scripts/release_version.py next).
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if [ "$#" -ne 1 ]; then
  echo "Usage: scripts/build_desktop_android.sh OUTPUT_APK" >&2
  exit 2
fi
case $1 in
  /*) output=$1 ;;
  *) output=$(pwd)/$1 ;;
esac

# Pinned: the NDK (as scripts/toolchain.py), the SDK packages, the app.
ndk_revision=29.0.14206865
# NDK r29's lowest API level; SDL 2.32 itself would go down to 19.
min_api=21
target_api=37
platform_package="platforms;android-37.0"
platform_dir=android-37.0
build_tools_version=37.0.0
app_id=fi.wekan.wena

fail() {
  echo "build_desktop_android: $*" >&2
  exit 1
}

if [ "$(basename -- "$(dirname -- "$root_dir")")" = .tools ]; then
  tools=$(dirname -- "$root_dir")
else
  tools=$root_dir/.tools
fi
work=$root_dir/.tools/release/android-arm64-apk
cache=${WENA_RELEASE_CACHE:-$root_dir/.tools/cache}
jobs=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)
case $(uname -s) in
  Darwin) host=macos; prebuilt=darwin-x86_64 ;;
  Linux) host=linux; prebuilt=linux-x86_64 ;;
  *) fail "builds on Linux x86_64 or macOS only (the NDK's hosts here)" ;;
esac
if [ "$host" = linux ] && [ "$(uname -m)" != x86_64 ]; then
  fail "the Android NDK has no Linux $(uname -m) compiler; build on Linux x86_64 or macOS"
fi

# --- The NDK.
ndk_found() {
  [ -n "$1" ] && [ -f "$1/source.properties" ] &&
    grep -q "^Pkg.Revision *= *$ndk_revision\$" "$1/source.properties"
}
ndk=
for candidate in "${ANDROID_NDK_ROOT:-}" "${ANDROID_NDK_HOME:-}" \
    "${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}/ndk/$ndk_revision" "$tools/android-ndk-r29"; do
  if ndk_found "$candidate"; then ndk=$candidate; break; fi
done
[ -n "$ndk" ] || fail "Android NDK r29 ($ndk_revision) not found. Looked in ANDROID_NDK_ROOT \
(${ANDROID_NDK_ROOT:-unset}), ANDROID_NDK_HOME (${ANDROID_NDK_HOME:-unset}), \
\$ANDROID_HOME/ndk/$ndk_revision and $tools/android-ndk-r29. Install it with \
'sdkmanager \"ndk;$ndk_revision\"' or './build.sh build android-arm64', or set ANDROID_NDK_ROOT."
ndk_bin=$ndk/toolchains/llvm/prebuilt/$prebuilt/bin
cc="$ndk_bin/aarch64-linux-android$min_api-clang"
readelf=$ndk_bin/llvm-readelf
[ -x "$cc" ] && [ -x "$readelf" ] || fail "the NDK at $ndk has no $prebuilt aarch64 compiler"

# --- The JDK.
if [ -n "${JAVA_HOME:-}" ]; then
  java_bin=$JAVA_HOME/bin
  [ -x "$java_bin/javac" ] || fail "JAVA_HOME ($JAVA_HOME) has no bin/javac; set JAVA_HOME to a JDK 17 or newer"
else
  javac_path=$(command -v javac 2>/dev/null) || fail "no JDK: set JAVA_HOME to a JDK 17 or newer (javac, java, keytool)"
  java_bin=$(dirname -- "$javac_path")
fi
java_major=$("$java_bin/javac" -version 2>&1 | sed -n 's/^javac \([0-9][0-9]*\).*/\1/p')
[ -n "$java_major" ] && [ "$java_major" -ge 17 ] ||
  fail "$java_bin/javac is not a JDK 17 or newer (d8 and apksigner need 17); set JAVA_HOME"
JAVA_HOME=$(dirname -- "$java_bin")
PATH=$java_bin:$PATH
export JAVA_HOME PATH

# --- The SDK: the pinned platform and build-tools, installed when missing.
sdk=${ANDROID_HOME:-${ANDROID_SDK_ROOT:-$tools/android-sdk}}
android_jar=$sdk/platforms/$platform_dir/android.jar
build_tools=$sdk/build-tools/$build_tools_version
if [ ! -f "$android_jar" ] || [ ! -x "$build_tools/aapt2" ]; then
  sdkmanager=$sdk/cmdline-tools/latest/bin/sdkmanager
  if [ ! -x "$sdkmanager" ] && [ "$sdk" = "$tools/android-sdk" ]; then
    case $host-$(uname -m) in
      linux-*) tools_pin=android-cmdline-tools-linux ;;
      macos-arm64) tools_pin=android-cmdline-tools-macos-arm64 ;;
      *) tools_pin=android-cmdline-tools-macos-x86_64 ;;
    esac
    echo "Installing Android cmdline-tools into $sdk/cmdline-tools/latest" >&2
    archive=$(python3 "$root_dir/scripts/fetch_release_dependency.py" "$tools_pin" "$cache")
    rm -rf "$sdk/cmdline-tools/latest" "$sdk/cmdline-tools/.unpack"
    mkdir -p "$sdk/cmdline-tools/.unpack"
    python3 - "$archive" "$sdk/cmdline-tools/.unpack" <<'PY'
import os, sys, zipfile
with zipfile.ZipFile(sys.argv[1]) as bundle:
    for info in bundle.infolist():
        path = bundle.extract(info, sys.argv[2])
        mode = info.external_attr >> 16
        if mode:
            os.chmod(path, mode & 0o777)
PY
    mv "$sdk/cmdline-tools/.unpack/cmdline-tools" "$sdk/cmdline-tools/latest"
    rm -rf "$sdk/cmdline-tools/.unpack"
  fi
  [ -x "$sdkmanager" ] || fail "the Android SDK at $sdk lacks $platform_package and build-tools;$build_tools_version, \
and has no cmdline-tools/latest/bin/sdkmanager to install them. Set ANDROID_HOME to an SDK with them, or \
unset it to have them installed in $tools/android-sdk."
  echo "Installing $platform_package and build-tools;$build_tools_version into $sdk" >&2
  "$sdkmanager" --sdk_root="$sdk" --install "$platform_package" "build-tools;$build_tools_version" >&2 ||
    fail "sdkmanager could not install them (if it asks for licences: yes | \"$sdkmanager\" --sdk_root=\"$sdk\" --licenses)"
fi
[ -f "$android_jar" ] && [ -x "$build_tools/aapt2" ] && [ -x "$build_tools/d8" ] &&
  [ -x "$build_tools/zipalign" ] && [ -x "$build_tools/apksigner" ] ||
  fail "the Android SDK at $sdk is missing $platform_package or build-tools;$build_tools_version"

# A release key, when one is named, must be usable before anything is built.
if [ -n "${WENA_ANDROID_KEYSTORE:-}" ]; then
  [ -f "$WENA_ANDROID_KEYSTORE" ] || fail "WENA_ANDROID_KEYSTORE ($WENA_ANDROID_KEYSTORE) is not a file"
  [ -n "${WENA_ANDROID_KEYSTORE_PASSWORD:-}" ] && [ -n "${WENA_ANDROID_KEY_ALIAS:-}" ] ||
    fail "WENA_ANDROID_KEYSTORE needs WENA_ANDROID_KEYSTORE_PASSWORD and WENA_ANDROID_KEY_ALIAS"
fi

# --- Version: vMAJOR.MINOR -> versionName MAJOR.MINOR, versionCode MAJOR*100+MINOR.
version=${WENA_VERSION:-$(python3 "$root_dir/scripts/release_version.py" next)}
case $version in
  v[0-9]*.[0-9][0-9]) ;;
  *) fail "WENA_VERSION must look like v0.01 (got $version)" ;;
esac
version_name=${version#v}
version_major=${version_name%%.*}
version_minor=${version_name#*.}
version_code=$((version_major * 100 + ${version_minor#0}))
[ "$version_code" -ge 1 ] || version_code=1

mkdir -p "$work" "$(dirname -- "$output")"
sdl_archive=$(python3 "$root_dir/scripts/fetch_release_dependency.py" sdl2 "$cache")
sqlite_archive=$(python3 "$root_dir/scripts/fetch_release_dependency.py" sqlite "$cache")

# --- SDL2: SDL's own Android.mk, as a static library (with its cpufeatures).
sdl_src=$work/sdl-src
sdl_obj=$work/sdl-ndk/obj/local/arm64-v8a
if [ ! -f "$sdl_obj/libSDL2.a" ] || [ ! -f "$sdl_obj/libcpufeatures.a" ]; then
  rm -rf "$sdl_src" "$work/sdl-ndk"
  mkdir -p "$sdl_src" "$work/sdl-ndk"
  tar -xzf "$sdl_archive" -C "$sdl_src" --strip-components=1
  "$ndk/ndk-build" -C "$work/sdl-ndk" NDK_PROJECT_PATH="$work/sdl-ndk" \
    APP_BUILD_SCRIPT="$sdl_src/Android.mk" APP_ABI=arm64-v8a APP_PLATFORM=android-$min_api \
    APP_STL=c++_static APP_MODULES=SDL2_static NDK_OUT="$work/sdl-ndk/obj" \
    NDK_LIBS_OUT="$work/sdl-ndk/libs" -j"$jobs" > "$work/sdl-ndk-build.log" 2>&1 ||
    { tail -40 "$work/sdl-ndk-build.log" >&2; fail "SDL2 did not build (log: $work/sdl-ndk-build.log)"; }
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
"$cc" -O2 -fPIC -DSQLITE_THREADSAFE=1 -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_DQS=0 \
  -c "$sqlite_dir/sqlite3.c" -o "$sqlite_dir/sqlite3.o"

# --- libmain.so: build_desktop.sh's sources and flags, as a shared library.
# SDL2 whole, so its JNI entry points (JNI_OnLoad, Java_org_libsdl_app_*) are
# in the library SDLActivity loads; its C++ (hidapi) with the static libc++.
apk_dir=$work/apk
rm -rf "$apk_dir"
mkdir -p "$apk_dir/lib/arm64-v8a" "$apk_dir/classes" "$apk_dir/res/mipmap-xxxhdpi"
library=$apk_dir/lib/arm64-v8a/libmain.so
WENA_CC=$cc \
WENA_CFLAGS="-O2 -fPIC" \
WENA_SDL_CFLAGS="-I$sdl_src/include" \
WENA_SDL_LIBS="-Wl,--whole-archive $sdl_obj/libSDL2.a -Wl,--no-whole-archive $sdl_obj/libcpufeatures.a" \
WENA_SQLITE_CFLAGS="-isystem $sqlite_dir" \
WENA_SQLITE_LIBS="$sqlite_dir/sqlite3.o" \
WENA_LDFLAGS="-shared -Wl,-soname,libmain.so -Wl,--no-undefined -Wl,--build-id=none -s \
-lc++_static -lc++abi -ldl -lGLESv1_CM -lGLESv2 -lOpenSLES -llog -landroid" \
  sh "$root_dir/scripts/build_desktop.sh" "$library"

# Prove it: an AArch64 shared library that loads nothing but Android's own.
"$readelf" -h "$library" | grep -Eq 'Type:[[:space:]]+DYN' || fail "libmain.so is not a shared library"
"$readelf" -h "$library" | grep -Eq 'Machine:[[:space:]]+AArch64' || fail "libmain.so is not AArch64"
needed=$("$readelf" -d "$library" | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p')
for lib in $needed; do
  case $lib in
    libc.so|libm.so|libdl.so|liblog.so|libandroid.so|libGLESv1_CM.so|libGLESv2.so|libEGL.so|libOpenSLES.so) ;;
    *) fail "libmain.so needs $lib, which is not an Android system library" ;;
  esac
done
for symbol in SDL_main JNI_OnLoad Java_org_libsdl_app_SDLActivity_nativeRunMain; do
  "$readelf" --dyn-syms -W "$library" | grep -q " $symbol\$" || fail "libmain.so does not export $symbol"
done

# --- Classes: SDL's Java glue and WenaActivity, then dex.
find "$sdl_src/android-project/app/src/main/java" "$root_dir/client/platform/android/java" \
  -name '*.java' | sort > "$work/java-sources.txt"
"$java_bin/javac" -encoding UTF-8 --release 8 -nowarn -Xlint:-options -classpath "$android_jar" \
  -d "$apk_dir/classes" @"$work/java-sources.txt" > "$work/javac.log" 2>&1 ||
  { cat "$work/javac.log" >&2; fail "javac failed"; }
find "$apk_dir/classes" -name '*.class' | sort > "$work/classes.txt"
"$build_tools/d8" --release --min-api "$min_api" --lib "$android_jar" \
  --output "$apk_dir" @"$work/classes.txt"

# --- Manifest and resources.
cp -R "$root_dir/client/platform/android/res/values" "$apk_dir/res/"
python3 "$root_dir/scripts/mobile_icon.py" 192 "$apk_dir/res/mipmap-xxxhdpi/ic_launcher.png"
"$build_tools/aapt2" compile --dir "$apk_dir/res" -o "$apk_dir/resources.zip"
"$build_tools/aapt2" link -o "$apk_dir/unsigned.apk" -I "$android_jar" \
  --manifest "$root_dir/client/platform/android/AndroidManifest.xml" \
  --min-sdk-version "$min_api" --target-sdk-version "$target_api" \
  --version-code "$version_code" --version-name "$version_name" "$apk_dir/resources.zip"
python3 - "$apk_dir/unsigned.apk" "$apk_dir" <<'PY'
import sys, zipfile
from pathlib import Path
apk, base = sys.argv[1], Path(sys.argv[2])
with zipfile.ZipFile(apk, "a", zipfile.ZIP_DEFLATED) as bundle:
    for relative in ("classes.dex", "lib/arm64-v8a/libmain.so"):
        info = zipfile.ZipInfo(relative, date_time=(1981, 1, 1, 0, 0, 0))
        info.compress_type = zipfile.ZIP_DEFLATED
        info.external_attr = 0o644 << 16
        bundle.writestr(info, (base / relative).read_bytes())
PY
"$build_tools/zipalign" -P 16 -f 4 "$apk_dir/unsigned.apk" "$apk_dir/aligned.apk"

# --- Signing.
if [ -n "${WENA_ANDROID_KEYSTORE:-}" ]; then
  keystore=$WENA_ANDROID_KEYSTORE
  alias=$WENA_ANDROID_KEY_ALIAS
  WENA_ANDROID_KEY_PASSWORD=${WENA_ANDROID_KEY_PASSWORD:-$WENA_ANDROID_KEYSTORE_PASSWORD}
  export WENA_ANDROID_KEYSTORE_PASSWORD WENA_ANDROID_KEY_PASSWORD
  store_pass=env:WENA_ANDROID_KEYSTORE_PASSWORD
  key_pass=env:WENA_ANDROID_KEY_PASSWORD
else
  keystore=$work/debug.keystore
  alias=wenadebug
  store_pass=pass:android
  key_pass=pass:android
  if [ ! -f "$keystore" ]; then
    "$java_bin/keytool" -genkeypair -keystore "$keystore" -storetype PKCS12 -storepass android \
      -keypass android -alias "$alias" -keyalg RSA -keysize 2048 -validity 10000 \
      -dname "CN=Wena debug, O=Wena, C=FI" > "$work/keytool.log" 2>&1 ||
      { cat "$work/keytool.log" >&2; fail "keytool could not make a debug key"; }
  fi
  cat >&2 <<EOF
WARNING: no WENA_ANDROID_KEYSTORE given, so $output is signed with a debug key
made in $keystore. Android installs an update only over an APK signed with the
same key: this one will not update an install signed with any other key, and
an APK from another build folder will not update this one. Set
WENA_ANDROID_KEYSTORE, WENA_ANDROID_KEYSTORE_PASSWORD and WENA_ANDROID_KEY_ALIAS
to sign a release.
EOF
fi
"$build_tools/apksigner" sign --ks "$keystore" --ks-key-alias "$alias" \
  --ks-pass "$store_pass" --key-pass "$key_pass" --min-sdk-version "$min_api" \
  --out "$output" "$apk_dir/aligned.apk"
rm -f "$output.idsig"
"$build_tools/apksigner" verify --min-sdk-version "$min_api" "$output"
"$build_tools/zipalign" -c -P 16 4 "$output"
echo "Android APK: $output ($app_id $version_name, code $version_code, min SDK $min_api, target SDK $target_api)"
