#!/usr/bin/env python3
"""What build.sh and build.bat install before a build, per computer.

The package managers, which() and Docker are replaced here, so nothing is
installed: each case says which commands exist, and installing a package
makes the commands it provides exist."""

import hashlib
import importlib.util
import io
import os
from pathlib import Path
import stat
import sys
import tempfile
import zipfile


ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("toolchain", ROOT / "scripts" / "toolchain.py")
toolchain = importlib.util.module_from_spec(spec)
spec.loader.exec_module(toolchain)

# What each package name provides once installed.
PROVIDES = {
    "gcc": {"gcc", "cc"}, "file": {"file"}, "binutils": {"readelf", "objdump"},
    "gcc-aarch64-linux-gnu": {"aarch64-linux-gnu-gcc"}, "libc6-dev-arm64-cross": set(),
    "libc6-dev-armhf-cross": set(), "libc6-dev-amd64-cross": set(),
    "gcc-arm-linux-gnueabihf": {"arm-linux-gnueabihf-gcc"},
    "gcc-x86-64-linux-gnu": {"x86_64-linux-gnu-gcc"},
    "mingw-w64": {"x86_64-w64-mingw32-gcc", "x86_64-w64-mingw32-objdump", "i686-w64-mingw32-gcc"},
    "gcc-mingw-w64-x86-64": {"x86_64-w64-mingw32-gcc"},
    "binutils-mingw-w64-x86-64": {"x86_64-w64-mingw32-objdump"},
    "mingw64-gcc": {"x86_64-w64-mingw32-gcc"}, "mingw64-binutils": {"x86_64-w64-mingw32-objdump"},
    "gcc-mingw-w64-i686": {"i686-w64-mingw32-gcc"}, "binutils-mingw-w64-i686": {"i686-w64-mingw32-objdump"},
    "mingw32-gcc": {"i686-w64-mingw32-gcc"}, "mingw32-binutils": {"i686-w64-mingw32-objdump"},
    "make": {"make"}, "cmake": {"cmake"}, "openjdk@17": {"javac"}, "openjdk-17-jdk-headless": {"javac"},
    "java-17-openjdk-devel": {"javac"},
    "mingw": {"gcc", "objdump", "readelf"}, "BrechtSanders.WinLibs.POSIX.UCRT": {"gcc", "objdump", "readelf"},
    "docker-desktop": {"docker"}, "docker.io": {"docker"}, "moby-engine": {"docker"},
    "Docker.DockerDesktop": {"docker"}, "git": {"git", "sh", "file"}, "Git.Git": {"git", "sh", "file"},
    "sdl2": {"sdl2-config"}, "libsdl2-dev": {"sdl2-config"}, "SDL2-devel": {"sdl2-config"},
}


class Completed:
    def __init__(self, returncode=0):
        self.returncode = returncode


class Computer:
    """A fake computer: the commands it has, and what was run on it."""

    def __init__(self, host, commands=(), managers=("brew", "apt-get", "dnf", "choco", "winget"),
                 docker_running=True, sdks=("macosx",), failing=()):
        self.host = host
        self.commands = set(commands) | set(managers)
        self.docker_running = docker_running
        self.sdks, self.failing = set(sdks), set(failing)
        self.ran = []

    def which(self, command, path=None):
        return "/fake/" + command if command in self.commands else None

    def run(self, command, **kwargs):
        self.ran.append(command)
        words = [word for word in command if word != "sudo"]
        if words[:2] in (["brew", "install"], ["apt-get", "install"], ["dnf", "install"],
                         ["choco", "install"], ["winget", "install"]):
            for name in words[2:]:
                if name in self.failing:
                    return Completed(1)
                self.commands |= PROVIDES.get(name, set())
            return Completed(0)
        if words[:2] == ["docker", "info"]:
            return Completed(0 if self.docker_running else 1)
        if words[:2] == ["docker", "image"]:
            return Completed(1)
        if words[:3] == ["xcrun", "--sdk", words[2] if len(words) > 2 else ""]:
            env = kwargs.get("env") or {}
            selected = words[2] in self.sdks or "DEVELOPER_DIR" in env and words[2] == "iphoneos" and "xcode" in self.sdks
            return Completed(0 if selected else 1)
        return Completed(0)

    def builder(self, allowed=True):
        out = io.StringIO()
        installer = toolchain.Installer(self.host, self.run, self.which, allowed, out)
        return toolchain.Builder(self.host, installer, ROOT, self.run, self.which)

    def installs(self):
        return [command for command in self.ran
                if any(word == "install" for word in command) and "--sdk" not in command]


def prepare(computer, target, allowed=True):
    return toolchain.prepare(target, computer.builder(allowed))


def unavailable(computer, target, message, allowed=True):
    try:
        prepare(computer, target, allowed)
    except toolchain.Unavailable as error:
        assert message in str(error), (target, str(error))
        return
    raise AssertionError(f"{target} was buildable: expected {message!r}")


UBUNTU = toolchain.Host("linux", "amd64", "debian")
UBUNTU_ARM = toolchain.Host("linux", "arm64", "debian")
FEDORA = toolchain.Host("linux", "amd64", "fedora")
MAC = toolchain.Host("macos", "arm64")
WINDOWS = toolchain.Host("windows", "amd64")


def test_host():
    assert toolchain.linux_family('ID=ubuntu\nID_LIKE=debian\n') == "debian"
    assert toolchain.linux_family('ID=debian\n') == "debian"
    assert toolchain.linux_family('ID="linuxmint"\nID_LIKE="ubuntu debian"\n') == "debian"
    assert toolchain.linux_family('ID=fedora\n') == "fedora"
    assert toolchain.linux_family('ID=almalinux\nID_LIKE="rhel centos fedora"\n') == "fedora"
    assert toolchain.linux_family('ID=arch\n') is None
    host = toolchain.current_host("Linux", "aarch64", "ID=ubuntu\n")
    assert (host.system, host.cpu, host.family) == ("linux", "arm64", "debian")
    assert toolchain.current_host("Darwin", "arm64").system == "macos"
    assert toolchain.current_host("Windows", "AMD64").cpu == "amd64"
    assert toolchain.tools_directory(Path("/r/.tools/wena")) == Path("/r/.tools")
    assert toolchain.tools_directory(Path("/src/wena")) == Path("/src/wena/.tools")


def commands(plan):
    return [" ".join(command) for command in plan.commands]


def test_package_managers():
    # Ubuntu and Debian: apt-get, updated once, through sudo.
    computer = Computer(UBUNTU)
    prepare(computer, "windows-i686")
    installs = computer.installs()
    assert computer.ran[0][-2:] == ["apt-get", "update"] and computer.ran.count(computer.ran[0]) == 1
    assert installs[0][-3:] == ["--yes", "gcc-mingw-w64-i686", "binutils-mingw-w64-i686"]
    assert all(command[0] == "sudo" for command in installs) or os.geteuid() == 0
    # Fedora: dnf.
    computer = Computer(FEDORA)
    prepare(computer, "windows-amd64")
    assert computer.installs()[0][-4:] == ["install", "--assumeyes", "mingw64-gcc", "mingw64-binutils"]
    # macOS: Homebrew, a cask for Docker Desktop.
    computer = Computer(MAC, {"xcrun"})
    prepare(computer, "linux-amd64")
    assert ["brew", "install", "--cask", "docker-desktop"] in computer.installs()
    # Windows: Chocolatey, else winget; Git for Windows first, for sh.
    computer = Computer(WINDOWS)
    prepare(computer, "amigaos-m68k")
    assert computer.installs()[0] == ["choco", "install", "--yes", "--no-progress", "git"]
    assert ["choco", "install", "--yes", "--no-progress", "docker-desktop"] in computer.installs()
    computer = Computer(WINDOWS, managers=("winget",))
    prepare(computer, "amigaos-m68k")
    assert computer.installs()[0][:5] == ["winget", "install", "--exact", "--id", "Git.Git"]
    # Nothing is installed twice, or at all when it is already there.
    computer = Computer(UBUNTU, {"docker"})
    prepare(computer, "linux-amd64")
    assert computer.installs() == []


def test_targets():
    # Linux: in the release workflow's own container, on any computer with Docker.
    for host in (UBUNTU, FEDORA, MAC):
        computer = Computer(host, {"docker", "xcrun"})
        plan = prepare(computer, "linux-armel")
        assert commands(plan) == [
            f"docker run --rm --platform linux/arm/v5 --volume {ROOT}:/w --workdir /w "
            f"--env WENA_OWNER={os.getuid()}:{os.getgid()} arm32v5/debian:bookworm "
            "sh scripts/build_desktop_release_container.sh linux-armel"], host
    # Docker on Linux needs QEMU for another CPU; not for its own, and Docker Desktop never.
    computer = Computer(UBUNTU_ARM, {"docker"})
    prepare(computer, "linux-arm64")
    assert computer.installs() == []
    computer = Computer(UBUNTU_ARM, {"docker"})
    prepare(computer, "linux-amd64")
    assert any("qemu-user-static" in command for command in computer.installs())
    computer = Computer(MAC, {"docker"})
    prepare(computer, "linux-s390x")
    assert computer.installs() == []
    # Windows: MinGW-w64 for the CPU, then the release build and its release file.
    for host, target, compiler in ((UBUNTU, "windows-amd64", "x86_64-w64-mingw32-gcc"),
                                   (FEDORA, "windows-i686", "i686-w64-mingw32-gcc"),
                                   (MAC, "windows-amd64", "x86_64-w64-mingw32-gcc")):
        computer = Computer(host)
        plan = prepare(computer, target)
        assert compiler in computer.commands, (host, target)
        executable = f"dist/release/{target}/wena.exe"
        assert commands(plan)[0] == f"sh scripts/build_desktop_release.sh {target} {executable}"
        assert commands(plan)[1].endswith(f"scripts/package_desktop_release.py binary {target} {executable} release")
    # Windows arm64: the pinned llvm-mingw, fetched by the build itself.
    computer = Computer(MAC, {"xcrun"})
    assert commands(prepare(computer, "windows-arm64"))[0].startswith("sh scripts/build_desktop_release.sh windows-arm64")
    assert computer.installs() == []
    # macOS: the Command Line Tools' SDK.
    computer = Computer(MAC, {"xcrun"})
    assert commands(prepare(computer, "macos-amd64"))[0] == \
        "sh scripts/build_desktop_release.sh macos-amd64 dist/release/macos-amd64/wena"
    # A BSD or Haiku builds on that system itself.
    from unittest.mock import patch
    with patch.object(toolchain.platform, "system", return_value="FreeBSD"):
        plan = prepare(Computer(toolchain.Host("freebsd", "amd64")), "freebsd-amd64")
    assert commands(plan) == ["sh scripts/build_desktop_release_vm.sh freebsd-amd64"]
    # iOS: Xcode, used without changing which developer directory is selected.
    xcode = Path("/Applications/Xcode.app")
    if xcode.is_dir():
        computer = Computer(MAC, {"xcrun"}, sdks=("macosx", "xcode"))
        plan = prepare(computer, "ios-arm64")
        assert plan.env["DEVELOPER_DIR"].endswith("/Contents/Developer")
    computer = Computer(MAC, {"xcrun"}, sdks=("macosx", "iphoneos"))
    assert prepare(computer, "ios-arm64").env == {}
    # iOS builds SDL with CMake; Homebrew installs it.
    assert ["brew", "install", "cmake"] in computer.installs()
    # Android: a JDK 17 from the system's packages, given to the build as JAVA_HOME.
    saved = os.environ.pop("JAVA_HOME", None)
    try:
        for host, package in ((UBUNTU, "openjdk-17-jdk-headless"), (FEDORA, "java-17-openjdk-devel")):
            computer = Computer(host)
            from unittest.mock import patch as patched
            with patched.object(toolchain, "ensure_ndk", return_value=Path("/ndk")):
                plan = prepare(computer, "android-arm64")
            assert any(package in command for command in computer.installs()), host
            assert "JAVA_HOME" in plan.env and plan.env["ANDROID_NDK_ROOT"] == "/ndk"
            assert commands(plan)[0] == "sh scripts/build_desktop_android.sh dist/release/android-arm64/wena.apk"
    finally:
        if saved is not None:
            os.environ["JAVA_HOME"] = saved
    # The Windows build gets sh and a python3 the release scripts can call.
    computer = Computer(WINDOWS, {"docker"})
    with tempfile.TemporaryDirectory() as temp:
        builder = computer.builder()
        builder.root = Path(temp) / "wena"
        plan = toolchain.prepare("amigaos-m68k", builder)
        shim = Path(temp) / "wena" / ".tools" / "shims" / "python3"
        assert shim.read_text(encoding="utf-8").startswith("#!/bin/sh\nexec ")
        assert str(shim.parent) in plan.path
    # Every target of the catalog has a plan.
    assert sorted(toolchain.TARGETS) == sorted(toolchain.catalog())


def test_unavailable():
    # Negative: each says why and how, and installs nothing it cannot use.
    unavailable(Computer(UBUNTU), "macos-arm64", "only on macOS")
    unavailable(Computer(WINDOWS), "ios-arm64", "only on macOS")
    unavailable(Computer(MAC, {"xcrun"}, sdks=("macosx",)), "ios-arm64", "install Xcode")
    computer = Computer(MAC, {"xcrun", "xcode-select"}, sdks=())
    unavailable(computer, "macos-amd64", "Command Line Tools")
    assert ["xcode-select", "--install"] in computer.ran
    unavailable(Computer(UBUNTU_ARM), "android-arm64", "no compiler for linux arm64")
    unavailable(Computer(UBUNTU), "plan9-amd64", "no build requirements")
    unavailable(Computer(MAC), "openbsd-arm64", "builds on OpenBSD arm64 itself")
    unavailable(Computer(UBUNTU), "haiku-amd64", "builds on Haiku amd64 itself")
    unavailable(Computer(toolchain.Host("linux", "amd64", None)), "windows-amd64", "Windows targets cross-compile")
    unavailable(Computer(MAC, managers=()), "windows-amd64", "https://brew.sh")
    unavailable(Computer(WINDOWS, managers=()), "amigaos-m68k", "Chocolatey")
    unavailable(Computer(WINDOWS, {"sh", "file"}), "windows-arm64", "llvm-mingw")
    # A failed install, and WENA_NO_INSTALL.
    computer = Computer(FEDORA, failing={"mingw64-gcc"})
    unavailable(computer, "windows-amd64", "could not install mingw64-gcc")
    computer = Computer(UBUNTU)
    unavailable(computer, "linux-amd64", "WENA_NO_INSTALL", allowed=False)
    assert computer.installs() == []
    # Choco needs an administrator terminal, and says so.
    computer = Computer(WINDOWS, failing={"git"})
    unavailable(computer, "amigaos-m68k", "administrator")


def test_ndk():
    host = toolchain.Host("linux", "amd64", "debian")
    assert set(toolchain.NDK_ARCHIVES) == {"linux", "macos", "windows"}
    for name, size, sha1 in toolchain.NDK_ARCHIVES.values():
        assert name.startswith("android-ndk-r29-") and size > 100000000 and len(sha1) == 40
    with tempfile.TemporaryDirectory() as temp:
        tools = Path(temp)
        archive = tools / "ndk.zip"
        with zipfile.ZipFile(archive, "w") as bundle:
            bundle.writestr("android-ndk-r29/source.properties", "Pkg.Desc = Android NDK\nPkg.Revision = 29.0.14206865\n")
            tool = zipfile.ZipInfo("android-ndk-r29/bin/clang")
            tool.external_attr = (stat.S_IFREG | 0o755) << 16
            bundle.writestr(tool, "#!/bin/sh\n")
            link = zipfile.ZipInfo("android-ndk-r29/bin/clang-19")
            link.external_attr = (stat.S_IFLNK | 0o777) << 16
            bundle.writestr(link, "clang")
        data = archive.read_bytes()
        saved = dict(toolchain.NDK_ARCHIVES)
        try:
            toolchain.NDK_ARCHIVES["linux"] = ("ndk.zip", len(data), hashlib.sha1(data).hexdigest())
            fetched = []

            def fetch(url):
                fetched.append(url)
                return io.BytesIO(data)

            ndk = toolchain.ensure_ndk(host, tools / "t", say=lambda text: None, fetch=fetch)
            assert ndk == tools / "t" / "android-ndk-r29" and fetched == [toolchain.NDK_URL + "ndk.zip"]
            assert toolchain.ndk_revision(ndk) == "29.0.14206865"
            if os.name != "nt":
                assert os.access(ndk / "bin" / "clang", os.X_OK)
                assert os.readlink(ndk / "bin" / "clang-19") == "clang"
            # Once there, it is not downloaded again.
            assert toolchain.ensure_ndk(host, tools / "t", say=lambda text: None, fetch=fetch) == ndk
            assert len(fetched) == 1
            # Negative: a download that is not Google's file installs nothing.
            toolchain.NDK_ARCHIVES["linux"] = ("ndk.zip", len(data), "0" * 40)
            try:
                toolchain.ensure_ndk(host, tools / "u", say=lambda text: None, fetch=lambda url: io.BytesIO(data))
            except toolchain.Unavailable as error:
                assert "SHA-1" in str(error)
            else:
                raise AssertionError("an unverified NDK was installed")
            assert not (tools / "u" / "android-ndk-r29").exists()
            assert not (tools / "u" / "downloads" / "ndk.partial").exists()
        finally:
            toolchain.NDK_ARCHIVES.clear()
            toolchain.NDK_ARCHIVES.update(saved)
        # Negative: an entry outside the archive's folder is refused.
        evil = tools / "evil.zip"
        with zipfile.ZipFile(evil, "w") as bundle:
            bundle.writestr("../outside", "x")
        try:
            toolchain.extract(evil, tools / "x")
        except toolchain.Unavailable:
            pass
        else:
            raise AssertionError("a path outside the folder was extracted")
        assert not (tools / "outside").exists()


def test_entry_points():
    # build.sh and build.bat make sure Python is there for everything else.
    shell = (ROOT / "build.sh").read_text(encoding="utf-8")
    for manager in ("brew install python", "apt-get install --yes python3", "dnf install --assumeyes python3"):
        assert manager in shell
    batch = (ROOT / "build.bat").read_text(encoding="utf-8")
    assert "choco install --yes --no-progress python" in batch and "Python.Python.3" in batch
    # cmd.exe misreads labels in a file without CRLF endings; build.bat has none.
    assert ":" + "install" not in batch and "goto" not in batch.lower()
    # The local Linux containers are the workflow's (tests/test_release_workflow.py checks it).
    assert set(toolchain.LINUX_CONTAINERS) == {t for t in toolchain.catalog() if t.startswith("linux-")}
    assert set(toolchain.LINUX_CONTAINERS[t][0] for t in toolchain.LINUX_CONTAINERS) <= set(toolchain.QEMU_CPUS)


def main():
    test_host()
    test_package_managers()
    test_targets()
    test_unavailable()
    test_ndk()
    test_entry_points()
    print("toolchain: ok")


if __name__ == "__main__":
    main()
