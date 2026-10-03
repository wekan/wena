#!/usr/bin/env python3
"""What building a target needs on this computer, installed when it is missing.

Each computer's own package manager is used: Homebrew on macOS, apt on Debian
and Ubuntu, dnf on Fedora, and Chocolatey (winget when there is no Chocolatey)
on Windows. The Android NDK is the one download that is not a package: the
same revision the release workflow installs, checked against Google's size and
SHA-1 and unpacked into .tools.

prepare(target) returns a Plan: the commands that build the target's release
file into release/ exactly as .github/workflows/release-all.yml does - a Linux
target in the same container image (under QEMU for another CPU), Windows with
MinGW-w64, macOS and iOS with Xcode - and what to add to their environment.
Unavailable says why a target cannot be built here at all, such as iOS without
Xcode or a BSD anywhere but on that BSD, and how to get it.

WENA_NO_INSTALL=1 checks without installing anything."""

import hashlib
import os
from pathlib import Path
import platform
import shutil
import stat
import subprocess
import sys
import time
import urllib.request
import zipfile


ROOT = Path(__file__).resolve().parents[1]

# The release workflow installs the same NDK with sdkmanager.
NDK_REVISION = "29.0.14206865"
NDK_DIRECTORY = "android-ndk-r29"
NDK_URL = "https://dl.google.com/android/repository/"
# From https://dl.google.com/android/repository/repository2-3.xml, ndk;29.0.14206865.
NDK_ARCHIVES = {
    "linux": ("android-ndk-r29-linux.zip", 783549481, "87e2bb7e9be5d6a1c6cdf5ec40dd4e0c6d07c30b"),
    "macos": ("android-ndk-r29-darwin.zip", 1049519838, "03d29fbb57e3c05a7d53597dd011d856c1456a4f"),
    "windows": ("android-ndk-r29-windows.zip", 833850862, "ab3bb30fbb9e6903666d60c55d11e78b04e07472"),
}
NDK_PREBUILT = {"linux": "linux-x86_64", "macos": "darwin-x86_64", "windows": "windows-x86_64"}

# The container each Linux target is built in: the same as release-all.yml's
# linux matrix (tests/test_toolchain.py checks), so a local build is the release build.
LINUX_CONTAINERS = {
    "linux-amd64": ("linux/amd64", "ubuntu:22.04"),
    "linux-arm64": ("linux/arm64", "ubuntu:22.04"),
    "linux-armhf": ("linux/arm/v7", "ubuntu:22.04"),
    "linux-armel": ("linux/arm/v5", "arm32v5/debian:bookworm"),
    "linux-i686": ("linux/386", "debian:bookworm"),
    "linux-riscv64": ("linux/riscv64", "ubuntu:22.04"),
    "linux-ppc64le": ("linux/ppc64le", "ubuntu:22.04"),
    "linux-s390x": ("linux/s390x", "ubuntu:22.04"),
    "linux-mips64le": ("linux/mips64le", "mips64le/debian:bookworm"),
}
# QEMU's name for a container platform's CPU, as registered in binfmt_misc.
QEMU_CPUS = {"linux/amd64": "x86_64", "linux/arm64": "aarch64", "linux/arm/v7": "arm",
             "linux/arm/v5": "arm", "linux/386": "i386", "linux/riscv64": "riscv64",
             "linux/ppc64le": "ppc64le", "linux/s390x": "s390x", "linux/mips64le": "mips64el"}
HOST_PLATFORMS = {"amd64": "linux/amd64", "arm64": "linux/arm64"}
# The system a BSD or Haiku target is built on, as platform.system() names it.
VM_SYSTEMS = {"freebsd": "FreeBSD", "netbsd": "NetBSD", "openbsd": "OpenBSD",
              "dragonflybsd": "DragonFly", "haiku": "Haiku"}

# A package per package manager. "--cask NAME" is a Homebrew cask; several
# names are separated by spaces. A manager missing from an entry has no
# package for it, and the target is then not buildable on that computer.
PACKAGES = {
    "gcc": {"apt": "gcc libc6-dev", "dnf": "gcc glibc-devel", "choco": "mingw",
            "winget": "BrechtSanders.WinLibs.POSIX.UCRT"},
    "file": {"apt": "file", "dnf": "file"},
    "binutils": {"apt": "binutils", "dnf": "binutils", "brew": "binutils", "choco": "mingw",
                 "winget": "BrechtSanders.WinLibs.POSIX.UCRT"},
    "mingw-w64": {"brew": "mingw-w64", "apt": "gcc-mingw-w64-x86-64 binutils-mingw-w64-x86-64",
                  "dnf": "mingw64-gcc mingw64-binutils", "choco": "mingw",
                  "winget": "BrechtSanders.WinLibs.POSIX.UCRT"},
    "mingw-w64-i686": {"brew": "mingw-w64", "apt": "gcc-mingw-w64-i686 binutils-mingw-w64-i686",
                       "dnf": "mingw32-gcc mingw32-binutils"},
    "make": {"apt": "make", "dnf": "make"},
    "jdk": {"brew": "openjdk@17", "apt": "openjdk-17-jdk-headless", "dnf": "java-17-openjdk-devel",
            "choco": "temurin17", "winget": "EclipseAdoptium.Temurin.17.JDK"},
    "cmake": {"brew": "cmake", "apt": "cmake", "dnf": "cmake", "choco": "cmake", "winget": "Kitware.CMake"},
    "docker": {"brew": "--cask docker-desktop", "apt": "docker.io", "dnf": "moby-engine",
               "choco": "docker-desktop", "winget": "Docker.DockerDesktop"},
    "qemu": {"apt": "qemu-user-static binfmt-support", "dnf": "qemu-user-static"},
    "git": {"brew": "git", "apt": "git", "dnf": "git", "choco": "git", "winget": "Git.Git"},
    "sdl2": {"brew": "sdl2", "apt": "libsdl2-dev", "dnf": "SDL2-devel"},
    "sqlite": {"apt": "libsqlite3-dev", "dnf": "sqlite-devel"},
    "msys2": {"choco": "msys2", "winget": "MSYS2.MSYS2"},
}

WINDOWS_DIRECTORIES = (
    r"C:\Program Files\Git\usr\bin", r"C:\Program Files\Git\bin",
    r"C:\ProgramData\mingw64\mingw64\bin",
    r"C:\Program Files\Docker\Docker\resources\bin",
)
MSYS2_ROOTS = (r"C:\tools\msys64", r"C:\msys64")
MACOS_DIRECTORIES = ("/opt/homebrew/opt/binutils/bin", "/usr/local/opt/binutils/bin",
                     "/opt/homebrew/opt/openjdk@17/bin", "/usr/local/opt/openjdk@17/bin",
                     "/opt/homebrew/bin", "/usr/local/bin")


class Unavailable(Exception):
    """This target cannot be built on this computer; the message says why."""


class Host:
    def __init__(self, system, cpu, family=None):
        self.system, self.cpu, self.family = system, cpu, family

    def __repr__(self):
        return f"Host({self.system!r}, {self.cpu!r}, {self.family!r})"


def linux_family(os_release):
    """debian, fedora or None, from the text of /etc/os-release."""
    words = set()
    for line in os_release.splitlines():
        key, _, value = line.partition("=")
        if key in ("ID", "ID_LIKE"):
            words.update(value.strip().strip('"').lower().split())
    if words & {"debian", "ubuntu"}:
        return "debian"
    if words & {"fedora", "rhel", "centos"}:
        return "fedora"
    return None


def current_host(system=None, machine=None, os_release=None):
    system = (system or platform.system()).lower()
    machine = (machine or platform.machine()).lower()
    cpu = {"x86_64": "amd64", "amd64": "amd64", "aarch64": "arm64", "arm64": "arm64",
           "armv7l": "armhf", "armv8l": "armhf"}.get(machine, machine)
    if system == "darwin":
        return Host("macos", cpu)
    if system == "windows":
        return Host("windows", cpu)
    if system == "linux":
        if os_release is None:
            try:
                os_release = Path("/etc/os-release").read_text(encoding="utf-8")
            except OSError:
                os_release = ""
        return Host("linux", cpu, linux_family(os_release))
    return Host(system, cpu)


def tools_directory(root=ROOT):
    """The .tools folder Wena is checked out in, or the one inside it otherwise."""
    root = Path(root)
    return root.parent if root.parent.name == ".tools" else root / ".tools"


class Plan:
    """What a build runs, and what it needs besides this process's environment."""

    def __init__(self):
        self.path = []        # directories put first on PATH
        self.env = {}         # variables set for the build
        self.commands = []    # run in order in the Wena checkout; empty for the desktop

    def add_path(self, directory):
        directory = str(directory)
        if directory not in self.path:
            self.path.append(directory)

    def environment(self, base=None):
        environment = dict(os.environ if base is None else base)
        environment.update(self.env)
        if self.path:
            environment["PATH"] = os.pathsep.join(self.path + [environment.get("PATH", "")])
        return environment


class Installer:
    def __init__(self, host, run=subprocess.run, which=shutil.which, allowed=None, out=None):
        self.host, self.run, self.which = host, run, which
        self.allowed = os.environ.get("WENA_NO_INSTALL", "") in ("", "0") if allowed is None else allowed
        self.out = out or sys.stdout
        self.updated = False
        self.manager = self._manager()

    def _manager(self):
        if self.host.system == "macos":
            return "brew" if self.which("brew") else None
        if self.host.system == "windows":
            return "choco" if self.which("choco") else "winget" if self.which("winget") else None
        if self.host.system == "linux":
            return {"debian": "apt", "fedora": "dnf"}.get(self.host.family)
        return None

    def say(self, text):
        print(text, file=self.out, flush=True)

    def privileged(self, command):
        if self.host.system == "linux" and hasattr(os, "geteuid") and os.geteuid() != 0:
            return ["sudo", *command]
        return command

    def commands(self, names):
        """The commands that install these package names."""
        if self.manager == "brew":
            if names[0] == "--cask":
                return [["brew", "install", "--cask", *names[1:]]]
            return [["brew", "install", *names]]
        if self.manager == "apt":
            update = [] if self.updated else [self.privileged(["apt-get", "update"])]
            self.updated = True
            return update + [self.privileged(["apt-get", "install", "--yes", *names])]
        if self.manager == "dnf":
            return [self.privileged(["dnf", "install", "--assumeyes", *names])]
        if self.manager == "choco":
            return [["choco", "install", "--yes", "--no-progress", *names]]
        if self.manager == "winget":
            return [["winget", "install", "--exact", "--id", name, "--silent",
                     "--accept-source-agreements", "--accept-package-agreements"] for name in names]
        raise AssertionError(self.manager)

    def install(self, package, why):
        spec = PACKAGES[package].get(self.manager) if self.manager else None
        if spec is None:
            raise Unavailable(f"{why} is missing, and {self.no_package_reason(package)}")
        if not self.allowed:
            raise Unavailable(f"{why} is missing (WENA_NO_INSTALL is set; install {spec} with {self.manager})")
        for command in self.commands(spec.split()):
            self.say("Installing " + why + ": " + " ".join(command))
            if self.run(command).returncode != 0:
                raise Unavailable(f"could not install {spec} with {self.manager}"
                                  + (" (run build.bat from an administrator terminal)"
                                     if self.manager == "choco" else ""))
        if self.host.system == "windows":
            refresh_windows_path()

    def no_package_reason(self, package):
        if self.manager is None:
            return {
                "macos": "Homebrew is not installed: https://brew.sh",
                "windows": "neither Chocolatey (https://chocolatey.org/install) nor winget is installed",
                "linux": "this Linux is neither Debian, Ubuntu nor Fedora; install it yourself",
            }.get(self.host.system, f"{self.host.system} has no supported package manager")
        return f"{self.manager} has no package for it"


def refresh_windows_path():
    """An installer adds itself to PATH in the registry; read it back."""
    try:
        import winreg
    except ImportError:
        return
    parts = []
    for hive, key in ((winreg.HKEY_LOCAL_MACHINE, r"SYSTEM\CurrentControlSet\Control\Session Manager\Environment"),
                      (winreg.HKEY_CURRENT_USER, "Environment")):
        try:
            with winreg.OpenKey(hive, key) as handle:
                parts += os.path.expandvars(winreg.QueryValueEx(handle, "Path")[0]).split(os.pathsep)
        except OSError:
            pass
    current = os.environ.get("PATH", "").split(os.pathsep)
    os.environ["PATH"] = os.pathsep.join(current + [part for part in parts if part and part not in current])


class Builder:
    """Finds and installs what one build needs, recording it in a Plan."""

    def __init__(self, host=None, installer=None, root=ROOT, run=subprocess.run, which=shutil.which):
        self.host = host or current_host()
        self.run, self.which, self.root = run, which, Path(root)
        self.installer = installer or Installer(self.host, run, which)
        self.plan = Plan()

    def directories(self):
        if self.host.system == "macos":
            return list(MACOS_DIRECTORIES)
        if self.host.system == "windows":
            links = Path(os.environ.get("LOCALAPPDATA", "")) / "Microsoft" / "WinGet" / "Links"
            return list(WINDOWS_DIRECTORIES) + [str(Path(root) / "mingw64" / "bin") for root in MSYS2_ROOTS] + [str(links)]
        return []

    def find(self, command):
        path = os.pathsep.join(self.plan.path + [os.environ.get("PATH", "")])
        found = self.which(command, path=path)
        if found:
            return found
        for directory in self.directories():
            found = self.which(command, path=directory)
            if found:
                self.plan.add_path(directory)
                return found
        return None

    def need(self, command, package, why=None):
        why = why or command
        if self.find(command):
            return
        self.installer.install(package, why)
        if not self.find(command):
            raise Unavailable(f"{why} is still not found after installing {package}; "
                              "open a new terminal and build again")

    def need_package(self, command, names, why):
        """A package named here rather than in PACKAGES (Debian's cross-compilers)."""
        PACKAGES.setdefault(names, {"apt": names})
        self.need(command, names, why)

    def ok(self, command, env=None):
        try:
            return self.run(command, capture_output=True, env=env).returncode == 0
        except OSError:
            return False

    # Hosts --------------------------------------------------------------

    def windows_shell(self):
        """Release scripts are sh scripts that call python3, on Windows too."""
        self.need("sh", "git", "sh (Git for Windows)")
        self.need("file", "git", "file (Git for Windows)")
        shims = tools_directory(self.root) / "shims"
        shims.mkdir(parents=True, exist_ok=True)
        python = Path(sys.executable).as_posix()
        (shims / "python3").write_text(f'#!/bin/sh\nexec "{python}" "$@"\n', encoding="utf-8")
        self.plan.add_path(shims)

    def xcode_sdk(self, sdk):
        """The Xcode or Command Line Tools developer directory that has this SDK."""
        if self.ok(["xcrun", "--sdk", sdk, "--show-sdk-path"]):
            return
        # Xcode can be installed without being selected; use it without changing the selection.
        for xcode in sorted(Path("/Applications").glob("Xcode*.app"), reverse=True):
            developer = str(xcode / "Contents" / "Developer")
            if self.ok(["xcrun", "--sdk", sdk, "--show-sdk-path"], dict(os.environ, DEVELOPER_DIR=developer)):
                self.plan.env["DEVELOPER_DIR"] = developer
                return
        if sdk == "macosx":
            self.run(["xcode-select", "--install"])
            raise Unavailable("the Xcode Command Line Tools are being installed in the window that "
                              "opened; build again when that has finished")
        raise Unavailable("the iOS SDK comes only with Xcode: install Xcode from the App Store, "
                          "open it once, and build again")

    def docker(self, platform="linux/amd64"):
        self.need("docker", "docker", "Docker")
        if not self.ok(["docker", "info"]):
            self.start_docker()
        # Docker Desktop runs other CPUs' containers itself; Docker on Linux needs QEMU.
        if self.host.system == "linux" and platform and platform != HOST_PLATFORMS.get(self.host.cpu) \
                and not Path(f"/proc/sys/fs/binfmt_misc/qemu-{QEMU_CPUS[platform]}").exists():
            self.installer.install("qemu", f"QEMU to run {platform} containers")

    def start_docker(self):
        if self.host.system == "macos":
            self.run(["open", "-a", "Docker"])
        elif self.host.system == "windows":
            desktop = Path(r"C:\Program Files\Docker\Docker\Docker Desktop.exe")
            if desktop.is_file():
                subprocess.Popen([str(desktop)])
        elif self.host.system == "linux":
            self.run(self.installer.privileged(["systemctl", "start", "docker"]))
        self.installer.say("Waiting for Docker to start ...")
        for _ in range(60):
            if self.ok(["docker", "info"]):
                return
            time.sleep(3)
        if self.host.system == "linux":
            raise Unavailable("Docker is not usable by this user: run "
                              "'sudo usermod -aG docker $USER', log in again, and build again")
        raise Unavailable("Docker did not start; start Docker Desktop and build again")

    # Targets: each adds the commands that build it into release/<its file>.

    def release_build(self, target, extension=""):
        """scripts/build_desktop_release.sh here, then the release file."""
        executable = f"dist/release/{target}/wena{extension}"
        self.plan.commands += [
            ["sh", "scripts/build_desktop_release.sh", target, executable],
            [sys.executable, "scripts/package_desktop_release.py", "binary", target, executable, "release"]]

    def linux(self, target):
        platform, image = LINUX_CONTAINERS[target]
        self.docker(platform)
        # Files written in the container belong to this user afterwards, not root.
        owner = ["--env", f"WENA_OWNER={os.getuid()}:{os.getgid()}"] if hasattr(os, "getuid") else []
        self.plan.commands.append(
            ["docker", "run", "--rm", "--platform", platform, "--volume", f"{self.root}:/w",
             "--workdir", "/w", *owner, image, "sh", "scripts/build_desktop_release_container.sh", target])

    def windows(self, target):
        cpu = target.split("-", 1)[1]
        if self.host.system == "windows":
            self.msys2_toolchain(cpu)
        elif self.host.system == "macos" or self.host.family in ("debian", "fedora"):
            if cpu == "amd64":
                self.need("x86_64-w64-mingw32-gcc", "mingw-w64")
            elif cpu == "i686":
                self.need("i686-w64-mingw32-gcc", "mingw-w64-i686")
            elif self.host.system == "linux" and self.host.cpu not in ("amd64", "arm64"):
                raise Unavailable("the pinned llvm-mingw for Windows arm64 runs on amd64 or arm64 Linux and macOS")
            # Windows arm64: scripts/build_desktop_release.sh fetches the pinned llvm-mingw.
            if self.host.system == "linux":
                self.need("make", "make")
        else:
            raise Unavailable("Windows targets cross-compile on macOS, Debian, Ubuntu, Fedora or "
                              "Windows (MSYS2)")
        self.release_build(target, ".exe")

    def msys2_toolchain(self, cpu):
        """MinGW-w64 gcc and make for a Windows target, from MSYS2."""
        if cpu == "arm64":
            raise Unavailable("Windows arm64 cross-compiles on macOS or Linux (llvm-mingw)")
        root = self.msys2_root()
        prefix = {"amd64": "mingw-w64-x86_64", "i686": "mingw-w64-i686"}[cpu]
        directory = {"amd64": "mingw64", "i686": "mingw32"}[cpu]
        compiler = {"amd64": "x86_64-w64-mingw32-gcc.exe", "i686": "i686-w64-mingw32-gcc.exe"}[cpu]
        if not (root / directory / "bin" / compiler).is_file() or not (root / "usr" / "bin" / "make.exe").is_file():
            command = [str(root / "usr" / "bin" / "bash.exe"), "-lc",
                       f"pacman -S --needed --noconfirm {prefix}-gcc make"]
            self.installer.say("Installing MinGW-w64 and make: " + " ".join(command))
            if self.run(command).returncode != 0:
                raise Unavailable("could not install MinGW-w64 and make with MSYS2's pacman")
        # MSYS2's sh and make run SDL's configure; its MinGW gcc compiles.
        self.plan.add_path(root / "usr" / "bin")
        self.plan.add_path(root / directory / "bin")

    def macos(self, target):
        if self.host.system != "macos":
            raise Unavailable("macOS targets build only on macOS (with the Xcode Command Line Tools)")
        self.xcode_sdk("macosx")
        self.release_build(target)

    def vm(self, target):
        system = target.split("-", 1)[0]
        cpu = target.split("-", 1)[1]
        name = VM_SYSTEMS[system]
        if platform.system() != name or self.host.cpu != cpu:
            raise Unavailable(f"{target} builds on {name} {cpu} itself (GitHub builds it in a "
                              f"{name} virtual machine)")
        self.plan.commands.append(["sh", "scripts/build_desktop_release_vm.sh", target])

    def amiga(self, target):
        self.docker()
        executable = f"dist/release/{target}/wena"
        self.plan.commands += [
            ["sh", "scripts/build_desktop_amiga.sh", target, executable],
            [sys.executable, "scripts/package_desktop_release.py", "binary", target, executable, "release"]]

    def ios(self, target):
        if self.host.system != "macos":
            raise Unavailable("iOS builds only on macOS with Xcode")
        self.xcode_sdk("iphoneos")
        self.need("cmake", "cmake")
        executable = f"dist/release/{target}/wena.ipa"
        self.plan.commands += [
            ["sh", "scripts/build_desktop_ios.sh", executable],
            [sys.executable, "scripts/package_desktop_release.py", "binary", target, executable, "release"]]

    def android(self, target):
        prebuilt = NDK_PREBUILT.get(self.host.system)
        if prebuilt is None or (self.host.system != "macos" and self.host.cpu != "amd64"):
            raise Unavailable(f"the Android NDK has no compiler for {self.host.system} {self.host.cpu}")
        given = os.environ.get("ANDROID_NDK_ROOT")
        if not (given and ndk_revision(Path(given)) == NDK_REVISION):
            self.plan.env["ANDROID_NDK_ROOT"] = str(ensure_ndk(self.host, tools_directory(self.root),
                                                               say=self.installer.say))
        self.jdk()
        # scripts/build_desktop_android.sh installs the pinned SDK packages it
        # needs, into .tools/android-sdk unless ANDROID_HOME names another SDK.
        executable = f"dist/release/{target}/wena.apk"
        self.plan.commands += [
            ["sh", "scripts/build_desktop_android.sh", executable],
            [sys.executable, "scripts/package_desktop_release.py", "binary", target, executable, "release"]]

    def jdk(self):
        """A JDK 17 or newer for javac, d8 and apksigner, as JAVA_HOME."""
        if os.environ.get("JAVA_HOME") and Path(os.environ["JAVA_HOME"], "bin", "javac").exists():
            return
        self.need("javac", "jdk", "a JDK 17")
        javac = Path(self.find("javac")).resolve()
        # Homebrew's openjdk@17 is not linked onto PATH; its home is beside its bin.
        home = javac.parent.parent
        brewed = home / "libexec" / "openjdk.jdk" / "Contents" / "Home"
        self.plan.env["JAVA_HOME"] = str(brewed if brewed.is_dir() else home)

    def desktop(self):
        if not (self.root / "third_party" / "nuklear" / "nuklear.h").is_file():
            self.need("git", "git")
            self.installer.say("Fetching third_party/nuklear")
            if self.run(["git", "-C", str(self.root), "submodule", "update", "--init",
                         "third_party/nuklear"]).returncode != 0:
                raise Unavailable("could not fetch the third_party/nuklear submodule")
        if self.host.system == "macos":
            self.xcode_sdk("macosx")
            self.need("sdl2-config", "sdl2", "SDL2")
        elif self.host.system == "linux" and self.host.family in ("debian", "fedora"):
            self.need("cc", "gcc")
            self.need("sdl2-config", "sdl2", "SDL2")
            if not any(Path(d, "sqlite3.h").is_file() for d in ("/usr/include", "/usr/local/include")):
                self.installer.install("sqlite", "SQLite")
        elif self.host.system == "windows":
            self.msys2()
        else:
            raise Unavailable(f"the desktop app has no known build requirements for {self.host.system}")

    def msys2_root(self):
        roots = [Path(root) for root in MSYS2_ROOTS]
        if not any((root / "usr" / "bin" / "bash.exe").is_file() for root in roots):
            self.installer.install("msys2", "MSYS2")
        root = next((root for root in roots if (root / "usr" / "bin" / "bash.exe").is_file()), None)
        if root is None:
            raise Unavailable("MSYS2 is still not found after installing it")
        return root

    def msys2(self):
        """SDL2, SQLite and gcc for the Windows desktop, from MSYS2's MinGW-w64."""
        root = self.msys2_root()
        bin_directory = root / "mingw64" / "bin"
        if not (bin_directory / "sdl2-config").is_file() or not (bin_directory / "gcc.exe").is_file():
            command = [str(root / "usr" / "bin" / "bash.exe"), "-lc",
                       "pacman -S --needed --noconfirm mingw-w64-x86_64-gcc "
                       "mingw-w64-x86_64-SDL2 mingw-w64-x86_64-sqlite3"]
            self.installer.say("Installing SDL2, SQLite and gcc: " + " ".join(command))
            if self.run(command).returncode != 0:
                raise Unavailable("could not install SDL2, SQLite and gcc with MSYS2's pacman")
        self.plan.add_path(bin_directory)
        self.plan.env["WENA_CC"] = "gcc"


JOBS = {"linux": Builder.linux, "bsd": Builder.vm, "macos": Builder.macos, "windows": Builder.windows,
        "amiga": Builder.amiga, "android": Builder.android, "ios": Builder.ios}


def catalog(root=ROOT):
    """target -> job, from config/targets.tsv."""
    result = {}
    for line in (Path(root) / "config" / "targets.tsv").read_text(encoding="utf-8").splitlines():
        if line and not line.startswith("#"):
            fields = line.split("\t")
            result[fields[0]] = fields[2]
    return result


TARGETS = {target: JOBS[job] for target, job in catalog().items()}


def prepare(target, builder=None):
    """Install what building target needs here; a Plan, or Unavailable."""
    builder = builder or Builder()
    if target != "desktop" and target not in TARGETS:
        raise Unavailable(f"no build requirements are known for {target}")
    if builder.host.system == "windows":
        builder.windows_shell()
    if target == "desktop":
        builder.desktop()
    else:
        TARGETS[target](builder, target)
    return builder.plan


def ndk_revision(directory):
    try:
        text = (Path(directory) / "source.properties").read_text(encoding="utf-8")
    except OSError:
        return None
    for line in text.splitlines():
        key, _, value = line.partition("=")
        if key.strip() == "Pkg.Revision":
            return value.strip()
    return None


def ensure_ndk(host, tools, say=print, fetch=urllib.request.urlopen):
    """The pinned NDK in tools, downloaded and verified once."""
    destination = Path(tools) / NDK_DIRECTORY
    if ndk_revision(destination) == NDK_REVISION:
        return destination
    name, size, sha1 = NDK_ARCHIVES[host.system]
    downloads = Path(tools) / "downloads"
    downloads.mkdir(parents=True, exist_ok=True)
    archive = downloads / name
    if not archive.is_file() or archive.stat().st_size != size or file_sha1(archive) != sha1:
        say(f"Downloading Android NDK r29 ({size // 1000000} MB): {NDK_URL}{name}")
        partial = archive.with_suffix(".partial")
        with fetch(NDK_URL + name) as response, open(partial, "wb") as output:
            shutil.copyfileobj(response, output, 1 << 20)
        if partial.stat().st_size != size or file_sha1(partial) != sha1:
            partial.unlink()
            raise Unavailable(f"{name} did not match Google's size and SHA-1; nothing was installed")
        partial.replace(archive)
    say(f"Unpacking {name} into {Path(tools)}")
    staging = Path(tools) / (NDK_DIRECTORY + ".partial")
    shutil.rmtree(staging, ignore_errors=True)
    extract(archive, staging)
    unpacked = staging / NDK_DIRECTORY
    if ndk_revision(unpacked) != NDK_REVISION:
        raise Unavailable(f"{name} does not contain NDK {NDK_REVISION}")
    shutil.rmtree(destination, ignore_errors=True)
    unpacked.replace(destination)
    shutil.rmtree(staging, ignore_errors=True)
    return destination


def file_sha1(path):
    digest = hashlib.sha1()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def extract(archive, into):
    """Unzip keeping executable bits and symbolic links, which zipfile drops."""
    into = Path(into).resolve()
    with zipfile.ZipFile(archive) as bundle:
        for info in bundle.infolist():
            target = (into / info.filename).resolve()
            if target != into and into not in target.parents:
                raise Unavailable(f"{archive} has an entry outside its folder: {info.filename}")
            mode = info.external_attr >> 16
            if info.is_dir():
                target.mkdir(parents=True, exist_ok=True)
                continue
            target.parent.mkdir(parents=True, exist_ok=True)
            if stat.S_ISLNK(mode) and os.name != "nt":
                os.symlink(bundle.read(info).decode("utf-8"), target)
                continue
            with bundle.open(info) as source, open(target, "wb") as output:
                shutil.copyfileobj(source, output, 1 << 20)
            if mode & 0o777 and os.name != "nt":
                os.chmod(target, mode & 0o777)


def main(argv):
    if len(argv) != 1:
        print("Usage: toolchain.py TARGET|desktop", file=sys.stderr)
        return 2
    try:
        plan = prepare(argv[0])
    except Unavailable as error:
        print(f"{argv[0]}: {error}", file=sys.stderr)
        return 1
    print(f"{argv[0]}: ready")
    for command in plan.commands:
        print("  " + " ".join(command))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
