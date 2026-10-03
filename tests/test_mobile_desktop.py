#!/usr/bin/env python3
"""The desktop on Android and iOS, checked without building anything.

scripts/build_desktop_android.sh makes wena-android-arm64.apk and
scripts/build_desktop_ios.sh wena-ios-arm64.ipa. Building them needs the NDK,
the Android SDK, a JDK and Xcode, so here only what can be checked without
them: the pins, the manifest and Info.plist, the platform branches in the C,
the icon, and that each script refuses what is missing with a message saying
what it needs and where it looked - before it downloads or builds anything.
"""

import json
import os
from pathlib import Path
import platform
import plistlib
import re
import shutil
import stat
import struct
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ElementTree
import zlib

ROOT = Path(__file__).resolve().parents[1]
ANDROID = ROOT / "scripts" / "build_desktop_android.sh"
IOS = ROOT / "scripts" / "build_desktop_ios.sh"
NDK_REVISION = "29.0.14206865"
ANDROID_NS = "{http://schemas.android.com/apk/res/android}"


def read(relative):
    return (ROOT / relative).read_text(encoding="utf-8")


def script_value(text, name):
    match = re.search(rf"^{name}=(\S+)$", text, re.MULTILINE)
    assert match, f"{name} is not pinned"
    return match[1].strip('"')


def test_pins():
    pins = json.loads(read("config/release-dependencies.json"))
    for name in ("sdl2", "sqlite", "android-cmdline-tools-linux",
                 "android-cmdline-tools-macos-arm64", "android-cmdline-tools-macos-x86_64"):
        pin = pins[name]
        assert re.fullmatch(r"[0-9a-f]{64}", pin["sha256"]), name
        assert pin["url"].startswith("https://"), name
        assert pin["license"] and pin["verified"], name
    assert pins["sdl2"]["version"] == "2.32.10"
    android = read("scripts/build_desktop_android.sh")
    ios = read("scripts/build_desktop_ios.sh")
    # The same NDK as scripts/toolchain.py installs; exact SDK packages.
    assert script_value(android, "ndk_revision") == NDK_REVISION
    assert f'NDK_REVISION = "{NDK_REVISION}"' in read("scripts/toolchain.py")
    assert re.fullmatch(r"platforms;android-\d+(\.\d+)?", script_value(android, "platform_package"))
    assert re.fullmatch(r"\d+\.\d+\.\d+", script_value(android, "build_tools_version"))
    assert script_value(android, "platform_package").endswith(script_value(android, "platform_dir"))
    assert script_value(android, "build_tools_version") in pins["android-cmdline-tools-linux"]["use"]
    assert script_value(android, "platform_package") in pins["android-cmdline-tools-linux"]["use"]
    assert int(script_value(android, "min_api")) == 21  # NDK r29's lowest
    assert int(script_value(android, "target_api")) >= 35
    assert script_value(android, "app_id") == "fi.wekan.wena"
    for text in (android, ios):
        # Only pinned, checked downloads, and build_desktop.sh's one source list.
        assert "fetch_release_dependency.py\" sdl2" in text and "fetch_release_dependency.py\" sqlite" in text
        assert 'sh "$root_dir/scripts/build_desktop.sh"' in text
        assert "client/features/board.c" not in text, "the source list is build_desktop.sh's"
        assert "curl " not in text and "wget " not in text
        assert "git push" not in text
    # Negative: nothing in the APK comes from Gradle or a Maven repository.
    assert "gradlew" not in android and "build.gradle" not in android and "mavenCentral" not in android


def test_android_manifest():
    tree = ElementTree.parse(ROOT / "client/platform/android/AndroidManifest.xml")
    manifest = tree.getroot()
    assert manifest.get("package") == "fi.wekan.wena"
    features = {item.get(ANDROID_NS + "glEsVersion") for item in manifest.iter("uses-feature")}
    assert "0x00020000" in features
    application = manifest.find("application")
    activities = application.findall("activity")
    assert [item.get(ANDROID_NS + "name") for item in activities] == [".WenaActivity"]
    activity = activities[0]
    assert activity.get(ANDROID_NS + "exported") == "true"
    assert activity.get(ANDROID_NS + "launchMode") == "singleInstance"
    assert "orientation" in activity.get(ANDROID_NS + "configChanges")
    actions = [item.get(ANDROID_NS + "name") for item in activity.iter("action")]
    categories = [item.get(ANDROID_NS + "name") for item in activity.iter("category")]
    assert actions == ["android.intent.action.MAIN"]
    assert categories == ["android.intent.category.LAUNCHER"]
    # Negative: a local board needs no permission at all - no network, no storage.
    assert manifest.findall("uses-permission") == []
    # Versions come from the build script, not from a second place here.
    assert manifest.find("uses-sdk") is None and ANDROID_NS + "versionCode" not in manifest.attrib
    strings = ElementTree.parse(ROOT / "client/platform/android/res/values/strings.xml").getroot()
    assert [item.text for item in strings if item.get("name") == "app_name"] == ["Wena"]
    java = read("client/platform/android/java/fi/wekan/wena/WenaActivity.java")
    assert "package fi.wekan.wena;" in java and "extends SDLActivity" in java
    # SDL2 is linked into libmain.so: no libSDL2.so is loaded.
    assert re.search(r'getLibraries\(\)\s*\{\s*return new String\[\] \{ "main" \};', java)
    assert 'getBooleanExtra("smoke", false)' in java and '"--smoke"' in java


def filled_plist(platform_name="iphoneos"):
    text = read("client/platform/ios/Info.plist")
    for key, value in (("@VERSION@", "0.01"), ("@PLATFORM@", "iPhoneOS"), ("@PLATFORM_NAME@", platform_name),
                       ("@SDK_VERSION@", "18.0"), ("@MINIMUM_OS@", "15.0")):
        text = text.replace(key, value)
    return plistlib.loads(text.encode("utf-8"))


def test_info_plist():
    info = filled_plist()
    assert info["CFBundleIdentifier"] == "fi.wekan.wena"
    assert info["CFBundleExecutable"] == "Wena" and info["CFBundlePackageType"] == "APPL"
    assert info["CFBundleShortVersionString"] == "0.01" and info["MinimumOSVersion"] == "15.0"
    assert info["LSRequiresIPhoneOS"] is True and info["UIDeviceFamily"] == [1, 2]
    assert "UILaunchScreen" in info or "UILaunchStoryboardName" in info
    assert "UIInterfaceOrientationPortrait" in info["UISupportedInterfaceOrientations"]
    assert "UIInterfaceOrientationLandscapeLeft" in info["UISupportedInterfaceOrientations"]
    assert len(info["UISupportedInterfaceOrientations~ipad"]) == 4
    # The UIScene life cycle the iOS 27 SDK requires, with scene.m's delegate.
    manifest = info["UIApplicationSceneManifest"]
    assert manifest["UIApplicationSupportsMultipleScenes"] is False
    delegate = manifest["UISceneConfigurations"]["UIWindowSceneSessionRoleApplication"][0]["UISceneDelegateClassName"]
    scene = read("client/platform/ios/scene.m")
    assert re.search(rf"@interface {delegate} : UIResponder <UIWindowSceneDelegate>", scene)
    assert "wena_window.windowScene = wena_scene" in scene
    # The icons the plist names are the ones the script draws.
    ios = read("scripts/build_desktop_ios.sh")
    for name in info["CFBundleIcons"]["CFBundlePrimaryIcon"]["CFBundleIconFiles"] + \
            info["CFBundleIcons~ipad"]["CFBundlePrimaryIcon"]["CFBundleIconFiles"]:
        assert f'"$app/{name}@' in ios, name
    # Negative: every placeholder is one the script fills in.
    placeholders = set(re.findall(r"@[A-Z_]+@", read("client/platform/ios/Info.plist")))
    placeholders.discard("@NAMES@")
    for placeholder in placeholders:
        assert f"s/{placeholder}/" in ios, placeholder
    assert "CODE_SIGN" not in ios and "codesign " not in ios
    assert "-Wl,-no_adhoc_codesign" in ios, "the .ipa is unsigned, as documented"


def test_c_platform_branches():
    desktop = read("client/desktop.c")
    debug_log = read("client/platform/debug_log.c")
    workspace = read("server/sqlite_workspace.c")
    # TargetConditionals.h exists only on Apple systems.
    for name, text in (("client/desktop.c", desktop), ("client/platform/debug_log.c", debug_log)):
        lines = text.splitlines()
        for number, line in enumerate(lines):
            if "TargetConditionals.h" in line:
                assert lines[number - 1].strip() == "#if defined(__APPLE__)", name
            if "TARGET_OS_IPHONE" in line and line.lstrip().startswith("#"):
                assert "defined(__APPLE__)" in line, f"{name}:{number + 1}"
    assert "#if defined(__ANDROID__) || DESKTOP_IOS" in desktop
    assert "#define DESKTOP_SYSTEM WENA_SYSTEM_MOBILE" in desktop
    assert 'SDL_GetPrefPath("wekan", "wena")' in desktop
    assert "SDL_GetPreferredLocales()" in desktop
    assert "wena_debug_log_open_data(" in desktop and "wena_ios_scene_attach(window)" in desktop
    # SDL.h renames main to SDL_main on both: desktop.c includes it, and on a
    # phone the desktop's main is wrapped by the SDL_main that reports failures.
    assert "#include <SDL.h>" in desktop
    assert "#define DESKTOP_MAIN desktop_main" in desktop and "#define DESKTOP_MAIN main" in desktop
    assert re.search(r"#if DESKTOP_MOBILE\n/\* SDL_main, called by SDLActivity", desktop)
    # Desktops keep what they had: HOME/APPDATA, the software renderer, text
    # input from the start, the hidden smoke window.
    assert "return wena_environment(DESKTOP_HOME, out, capacity);" in desktop
    assert "SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);" in desktop
    assert "#if !DESKTOP_MOBILE\n    SDL_StartTextInput();\n#endif" in desktop
    assert "smoke && !DESKTOP_MOBILE ? SDL_WINDOW_HIDDEN" in desktop
    # Android: logcat, no signal handlers of our own, and no hard link.
    assert "__android_log_vprint" in debug_log and "#include <android/log.h>" not in debug_log
    assert "return open_in_directory(1, 0);" in debug_log
    assert "#if defined(__ANDROID__)" in workspace and "rename(staging, path)" in workspace
    assert "link(staging, path)" in workspace, "other systems keep the atomic no-replace link"
    # Negative: no environment variable switches the smoke test on; argv does.
    for text in (desktop, read("client/platform/android/java/fi/wekan/wena/WenaActivity.java")):
        assert "WENA_SMOKE" not in text


def png_size(data):
    assert data[:8] == b"\x89PNG\r\n\x1a\n"
    width, height, depth, colour = struct.unpack(">IIBB", data[16:26])
    assert depth == 8 and colour == 2, "opaque RGB, as iOS requires"
    zlib.decompress(data[data.index(b"IDAT") + 4:data.index(b"IEND") - 8])
    return width, height


def test_icon():
    with tempfile.TemporaryDirectory() as temp:
        first, second = Path(temp, "a.png"), Path(temp, "b.png")
        for path in (first, second):
            subprocess.run([sys.executable, str(ROOT / "scripts/mobile_icon.py"), "180", str(path)], check=True)
        assert first.read_bytes() == second.read_bytes(), "the icon is the same on every build"
        assert png_size(first.read_bytes()) == (180, 180)
        # Negative: a size outside 16-1024 or no output is refused.
        for arguments in (["8", str(first)], ["2048", str(first)], ["big", str(first)], ["120"]):
            result = subprocess.run([sys.executable, str(ROOT / "scripts/mobile_icon.py"), *arguments],
                                    capture_output=True, text=True)
            assert result.returncode == 2 and "Usage" in result.stderr


def executable(path, body):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("#!/bin/sh\n" + body + "\n")
    path.chmod(path.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)


def run(script, arguments, env):
    base = {key: value for key, value in os.environ.items()
            if not key.startswith(("ANDROID_", "JAVA_HOME", "WENA_"))}
    base.update(env)
    return subprocess.run(["sh", str(script), *arguments], capture_output=True, text=True, env=base)


def test_android_refusals():
    with tempfile.TemporaryDirectory() as temp:
        temp = Path(temp)
        # A copy outside any checkout, so it finds no .tools of its own.
        checkout = temp / "checkout"
        (checkout / "scripts").mkdir(parents=True)
        script = checkout / "scripts" / ANDROID.name
        shutil.copy(ANDROID, script)
        result = run(script, [], {})
        assert result.returncode == 2 and "Usage" in result.stderr
        supported = platform.system() == "Darwin" or (platform.system() == "Linux" and platform.machine() == "x86_64")
        output = str(temp / "out.apk")
        result = run(script, [output], {"ANDROID_NDK_ROOT": str(temp / "missing")})
        assert result.returncode == 1, result.stderr
        if not supported:
            assert "NDK has no" in result.stderr or "builds on Linux x86_64 or macOS" in result.stderr
            return
        assert "Android NDK r29" in result.stderr and "ANDROID_NDK_ROOT" in result.stderr
        assert str(temp / "missing") in result.stderr, "it says where it looked"
        # A stand-in NDK: the right revision and an aarch64 compiler.
        ndk = temp / "ndk"
        (ndk / "source.properties").parent.mkdir(parents=True)
        (ndk / "source.properties").write_text(f"Pkg.Desc = Android NDK\nPkg.Revision = {NDK_REVISION}\n")
        prebuilt = "darwin-x86_64" if platform.system() == "Darwin" else "linux-x86_64"
        bin_dir = ndk / "toolchains/llvm/prebuilt" / prebuilt / "bin"
        executable(bin_dir / "aarch64-linux-android21-clang", "exit 1")
        executable(bin_dir / "llvm-readelf", "exit 1")
        # Negative: another NDK revision is not used.
        wrong = temp / "ndk-r28"
        shutil.copytree(ndk, wrong)
        (wrong / "source.properties").write_text("Pkg.Revision = 28.2.13676358\n")
        result = run(script, [output], {"ANDROID_NDK_ROOT": str(wrong)})
        assert result.returncode == 1 and "Android NDK r29" in result.stderr
        env = {"ANDROID_NDK_ROOT": str(ndk), "JAVA_HOME": str(temp / "no-jdk"), "PATH": "/usr/bin:/bin"}
        result = run(script, [output], env)
        assert result.returncode == 1 and "JAVA_HOME" in result.stderr and "bin/javac" in result.stderr
        jdk = temp / "jdk"
        executable(jdk / "bin" / "javac", 'echo "javac 11.0.24" >&2')
        env["JAVA_HOME"] = str(jdk)
        result = run(script, [output], env)
        assert result.returncode == 1 and "JDK 17" in result.stderr
        executable(jdk / "bin" / "javac", 'echo "javac 17.0.12"')
        env["ANDROID_HOME"] = str(temp / "sdk")
        result = run(script, [output], env)
        assert result.returncode == 1, result.stderr
        assert "platforms;android-" in result.stderr and "sdkmanager" in result.stderr
        assert str(temp / "sdk") in result.stderr
        # A stand-in SDK with the pinned packages: now the signing and version checks.
        text = ANDROID.read_text()
        sdk = temp / "sdk"
        jar = sdk / "platforms" / script_value(text, "platform_dir") / "android.jar"
        jar.parent.mkdir(parents=True)
        jar.write_bytes(b"")
        for tool in ("aapt2", "d8", "zipalign", "apksigner"):
            executable(sdk / "build-tools" / script_value(text, "build_tools_version") / tool, "exit 1")
        env["WENA_ANDROID_KEYSTORE"] = str(temp / "missing.keystore")
        result = run(script, [output], env)
        assert result.returncode == 1 and "WENA_ANDROID_KEYSTORE" in result.stderr and "not a file" in result.stderr
        keystore = temp / "release.keystore"
        keystore.write_bytes(b"")
        env["WENA_ANDROID_KEYSTORE"] = str(keystore)
        result = run(script, [output], env)
        assert result.returncode == 1 and "WENA_ANDROID_KEYSTORE_PASSWORD" in result.stderr
        del env["WENA_ANDROID_KEYSTORE"]
        env["WENA_VERSION"] = "1.0"
        result = run(script, [output], env)
        assert result.returncode == 1 and "WENA_VERSION must look like v0.01" in result.stderr
        assert not Path(output).exists()
        assert not (checkout / ".tools").exists(), "nothing was downloaded or built"


def test_ios_refusals():
    with tempfile.TemporaryDirectory() as temp:
        temp = Path(temp)
        checkout = temp / "checkout"
        (checkout / "scripts").mkdir(parents=True)
        script = checkout / "scripts" / IOS.name
        shutil.copy(IOS, script)
        for arguments in ([], ["--simulator"], ["a.ipa", "b.ipa"]):
            result = run(script, arguments, {})
            assert result.returncode == 2 and "Usage" in result.stderr, arguments
        output = str(temp / "out.ipa")
        if platform.system() != "Darwin":
            result = run(script, [output], {})
            assert result.returncode == 1 and "only on macOS with Xcode" in result.stderr
            return
        result = run(script, [output], {"DEVELOPER_DIR": str(temp / "no-xcode")})
        assert result.returncode == 1 and "SDK" in result.stderr and "DEVELOPER_DIR" in result.stderr
        assert not Path(output).exists() and not (checkout / ".tools").exists()


def test_registered():
    text = read("scripts/wena.py")
    assert "('mobile-desktop', 'test_mobile_desktop.py'," in text
    for script in (ANDROID, IOS):
        assert script.stat().st_mode & stat.S_IXUSR, f"{script.name} is executable"


def main():
    test_pins()
    test_android_manifest()
    test_info_plist()
    test_c_platform_branches()
    test_icon()
    test_android_refusals()
    test_ios_refusals()
    test_registered()
    print("mobile desktop checks passed")


if __name__ == "__main__":
    main()
