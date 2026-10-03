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
    "mingw-w64": {"x86_64-w64-mingw32-gcc", "x86_64-w64-mingw32-objdump"},
    "gcc-mingw-w64-x86-64": {"x86_64-w64-mingw32-gcc"},
    "binutils-mingw-w64-x86-64": {"x86_64-w64-mingw32-objdump"},
    "mingw64-gcc": {"x86_64-w64-mingw32-gcc"}, "mingw64-binutils": {"x86_64-w64-mingw32-objdump"},
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


def test_package_managers():
    # Ubuntu and Debian: apt-get, updated once, through sudo.
    computer = Computer(UBUNTU)
    prepare(computer, "linux-arm64")
    installs = computer.installs()
    assert computer.ran[0][-2:] == ["apt-get", "update"] and computer.ran.count(computer.ran[0]) == 1
    # A cross-compiler comes with its C library: without it there is no <stdio.h>.
    assert ["apt-get", "install", "--yes", "gcc-aarch64-linux-gnu", "libc6-dev-arm64-cross"] == installs[0][-5:]
    assert all(command[0] == "sudo" for command in installs) or os.geteuid() == 0
    # Fedora: dnf.
    computer = Computer(FEDORA)
    prepare(computer, "windows-amd64")
    assert computer.installs()[0][-4:] == ["install", "--assumeyes", "mingw64-gcc", "mingw64-binutils"]
    # macOS: Homebrew, a cask for Docker Desktop.
    computer = Computer(MAC, {"xcrun"})
    prepare(computer, "amigaos-m68k")
    assert ["brew", "install", "--cask", "docker-desktop"] in computer.installs()
    # Windows: Chocolatey, else winget.
    computer = Computer(WINDOWS)
    prepare(computer, "windows-amd64")
    assert computer.installs()[0] == ["choco", "install", "--yes", "--no-progress", "git"]
    assert ["choco", "install", "--yes", "--no-progress", "mingw"] in computer.installs()
    computer = Computer(WINDOWS, managers=("winget",))
    prepare(computer, "windows-amd64")
    assert computer.installs()[0][:4] == ["winget", "install", "--exact", "--id"]
    assert computer.installs()[0][4] == "Git.Git"
    # Nothing is installed twice, or at all when it is already there.
    computer = Computer(UBUNTU, {"gcc", "file", "readelf"})
    prepare(computer, "linux-amd64")
    assert computer.installs() == []


def test_targets():
    # Linux: the native gcc; Debian's cross-compilers; Ubuntu's in a container elsewhere.
    for computer, target in ((Computer(UBUNTU), "linux-amd64"), (Computer(UBUNTU_ARM), "linux-arm64"),
                             (Computer(FEDORA), "linux-amd64")):
        plan = prepare(computer, target)
        assert plan.container is None and {"gcc", "file", "readelf"} <= computer.commands
    computer = Computer(UBUNTU)
    assert prepare(computer, "linux-armhf").container is None
    assert "arm-linux-gnueabihf-gcc" in computer.commands
    for host in (FEDORA, MAC, WINDOWS):
        computer = Computer(host, {"xcrun"})
        target = "linux-arm64"
        plan = prepare(computer, target)
        assert plan.container.startswith("wena-build-linux-arm64:"), host
        build = next(command for command in computer.ran if command[:2] == ["docker", "build"])
        assert build[-1] == "-"
        command = plan.container_command(target, Path("/w"))
        assert command[:3] == ["docker", "run", "--rm"] and command[-2:] == ["sh", ".github/release/linux-arm64.sh"]
        assert "/w:/work" in command
    builds = []
    computer = Computer(MAC, {"xcrun"})
    original = computer.run
    computer.run = lambda command, **kwargs: (builds.append(kwargs.get("input")), original(command, **kwargs))[1]
    prepare(computer, "linux-armhf")
    dockerfile = next(text for text in builds if text)
    assert "gcc-arm-linux-gnueabihf libc6-dev-armhf-cross" in dockerfile
    assert "python3 file binutils" in dockerfile
    # Windows amd64: MinGW-w64 everywhere, the native one on Windows.
    for host, compiler in ((UBUNTU, "x86_64-w64-mingw32-gcc"), (FEDORA, "x86_64-w64-mingw32-gcc"),
                           (MAC, "x86_64-w64-mingw32-gcc"), (WINDOWS, "gcc")):
        computer = Computer(host)
        prepare(computer, "windows-amd64")
        assert compiler in computer.commands, host
    # AmigaOS and AROS: Docker, started when it is not running.
    computer = Computer(MAC, {"docker"}, docker_running=True)
    prepare(computer, "amigaos-m68k")
    assert computer.installs() == []
    computer = Computer(UBUNTU, {"docker", "readelf", "file"})
    prepare(computer, "aros-x86")
    assert computer.installs() == []
    # An arm64 Linux needs QEMU to run the amd64 compiler images.
    computer = Computer(UBUNTU_ARM, {"docker"})
    prepare(computer, "amigaos-m68k")
    assert any("qemu-user-static" in command for command in computer.installs())
    # iOS: Xcode, used without changing which developer directory is selected.
    computer = Computer(MAC, {"xcrun"}, sdks=("macosx", "xcode"))
    xcode = Path("/Applications/Xcode.app")
    if xcode.is_dir():
        plan = prepare(computer, "ios-arm64")
        assert plan.env["DEVELOPER_DIR"].endswith("/Contents/Developer")
    computer = Computer(MAC, {"xcrun"}, sdks=("macosx", "iphoneos"))
    assert prepare(computer, "ios-arm64").env == {}
    # The Windows build gets sh and a python3 the release scripts can call.
    computer = Computer(WINDOWS)
    with tempfile.TemporaryDirectory() as temp:
        builder = computer.builder()
        builder.root = Path(temp) / "wena"
        plan = toolchain.prepare("windows-amd64", builder)
        shim = Path(temp) / "wena" / ".tools" / "shims" / "python3"
        assert shim.read_text(encoding="utf-8").startswith("#!/bin/sh\nexec ")
        assert str(shim.parent) in plan.path


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
    unavailable(Computer(toolchain.Host("linux", "amd64", None)), "windows-amd64", "neither Debian")
    unavailable(Computer(MAC, managers=()), "windows-amd64", "https://brew.sh")
    unavailable(Computer(WINDOWS, managers=()), "windows-amd64", "Chocolatey")
    # A failed install, and WENA_NO_INSTALL.
    computer = Computer(FEDORA, failing={"mingw64-gcc"})
    unavailable(computer, "windows-amd64", "could not install mingw64-gcc")
    computer = Computer(UBUNTU)
    unavailable(computer, "linux-amd64", "WENA_NO_INSTALL", allowed=False)
    assert computer.installs() == []
    # Choco needs an administrator terminal, and says so.
    computer = Computer(WINDOWS, failing={"git"})
    unavailable(computer, "windows-amd64", "administrator")


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


def test_release_scripts():
    # The release scripts accept what is installed on each computer.
    windows = (ROOT / ".github" / "release" / "windows-amd64.sh").read_text(encoding="utf-8")
    assert 'test "$(gcc -dumpmachine)" = x86_64-w64-mingw32' in windows
    # file 5.41 (Ubuntu, macOS) and 5.46 (Fedora) word a PE executable differently.
    import re
    pattern = re.search(r"^file \"\$binary\" \| grep -Eq '([^']+)'", windows, re.MULTILINE).group(1)
    for output in ("PE32+ executable (console) x86-64, for MS Windows",
                   "PE32+ executable for MS Windows 5.02 (console), x86-64, 18 sections"):
        assert re.search(pattern, output), output
    assert not re.search(pattern, "PE32 executable (console) Intel 80386, for MS Windows")
    android = (ROOT / ".github" / "release" / "android-arm64.sh").read_text(encoding="utf-8")
    for prebuilt in toolchain.NDK_PREBUILT.values():
        assert prebuilt in android
    for target in toolchain.AMIGA_IMAGES:
        script = (ROOT / ".github" / "release" / f"{target}.sh").read_text(encoding="utf-8")
        assert script.count("docker run --rm") == script.count("--platform linux/amd64"), target
    # Every ready target has requirements.
    ready = [line.split("\t")[0] for line in (ROOT / "config" / "targets.tsv").read_text(encoding="utf-8").splitlines()
             if line and not line.startswith("#") and line.split("\t")[3] == "ready"]
    assert sorted(ready) == sorted(toolchain.TARGETS)
    # build.sh and build.bat make sure Python is there for everything else.
    shell = (ROOT / "build.sh").read_text(encoding="utf-8")
    for manager in ("brew install python", "apt-get install --yes python3", "dnf install --assumeyes python3"):
        assert manager in shell
    batch = (ROOT / "build.bat").read_text(encoding="utf-8")
    assert "choco install --yes --no-progress python" in batch and "Python.Python.3" in batch
    # cmd.exe misreads labels in a file without CRLF endings; build.bat has none.
    assert ":" + "install" not in batch and "goto" not in batch.lower()


def main():
    test_host()
    test_package_managers()
    test_targets()
    test_unavailable()
    test_ndk()
    test_release_scripts()
    print("toolchain: ok")


if __name__ == "__main__":
    main()
