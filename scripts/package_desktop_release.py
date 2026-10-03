#!/usr/bin/env python3
"""Release files for the desktop, one executable per platform.

    package_desktop_release.py binary TARGET EXECUTABLE OUTPUT_DIR
        -> OUTPUT_DIR/wena-desktop-TARGET[.exe] and its .sha256
    package_desktop_release.py notices OUTPUT_DIR
        -> OUTPUT_DIR/wena-desktop-notices.tar.gz and its .sha256

Each platform's executable is attached on its own, so a user downloads only
theirs; the .sha256 beside it is in the format `sha256sum -c` reads. The
licenses and provenance that go with every executable are one archive, with
a README listing the platforms and what each needs. Same inputs, same bytes.
"""

import gzip
import hashlib
import io
from pathlib import Path
import shutil
import sys
import tarfile

ROOT = Path(__file__).resolve().parents[1]
GLIBC_UBUNTU = "glibc 2.34 or newer (Ubuntu 22.04, Debian 12, Fedora 35 or newer) and an X11 or Wayland desktop"
GLIBC_DEBIAN = "glibc 2.36 or newer (Debian 12 or newer) and an X11 or Wayland desktop"
TARGETS = {
    "linux-amd64": ("Linux x86-64", GLIBC_UBUNTU),
    "linux-arm64": ("Linux ARM 64-bit", GLIBC_UBUNTU),
    "linux-armhf": ("Linux ARMv7 hard-float (Raspberry Pi OS 32-bit)", GLIBC_UBUNTU),
    "linux-riscv64": ("Linux RISC-V 64-bit", GLIBC_UBUNTU),
    "linux-ppc64le": ("Linux POWER little-endian", GLIBC_UBUNTU),
    "linux-s390x": ("Linux IBM Z", GLIBC_UBUNTU),
    "linux-i686": ("Linux x86 32-bit", GLIBC_DEBIAN),
    "linux-armel": ("Linux ARMv5 soft-float", GLIBC_DEBIAN),
    "linux-mips64le": ("Linux MIPS64 little-endian", GLIBC_DEBIAN),
    "macos-arm64": ("macOS Apple silicon", "macOS 11 or newer"),
    "macos-amd64": ("macOS Intel", "macOS 10.13 or newer"),
    "windows-amd64": ("Windows x86-64", "Windows 7 or newer"),
    "windows-i686": ("Windows x86 32-bit", "Windows 7 or newer"),
    "windows-arm64": ("Windows ARM 64-bit", "Windows 10 or newer"),
}
NOTICES = {
    "LICENSE": "LICENSE",
    "NUKLEAR-LICENSE": "third_party/nuklear/LICENSE",
    "I18N-PROVENANCE.md": "imports/i18n/README.md",
    "DEPENDENCIES.json": "config/dependencies-lock.json",
    "RELEASE-DEPENDENCIES.json": "config/release-dependencies.json",
    "DEPENDENCY-AUDIT.md": "docs/dependency-audit.md",
    "FONT-LICENSE.txt": "third_party/fonts/LICENSE-Roboto.txt",
    "FONT-PROVENANCE.json": "third_party/fonts/provenance.json",
    "FONT-README.md": "third_party/fonts/README.md",
}


def executable_name(target):
    return f"wena-desktop-{target}" + (".exe" if target.startswith("windows-") else "")


def write_checksum(path):
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    checksum = path.with_name(path.name + ".sha256")
    checksum.write_text(f"{digest}  {path.name}\n", encoding="ascii")
    return checksum


def binary(target, executable, output):
    if target not in TARGETS:
        raise ValueError(f"unknown desktop release target: {target}")
    executable = Path(executable)
    if not executable.is_file() or executable.stat().st_size == 0:
        raise ValueError(f"missing or empty desktop executable: {executable}")
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    path = output / executable_name(target)
    shutil.copyfile(executable, path)
    path.chmod(0o755)
    write_checksum(path)
    return path


def readme():
    rows = "\n".join(f"  {executable_name(target):34} {name}; needs {needs}"
                     for target, (name, needs) in TARGETS.items())
    return f"""Wena desktop

One executable per platform. SDL2 and SQLite are linked in; nothing else to
install. Download the one for your system, check it against its .sha256
(sha256sum -c), make it executable on Linux and macOS, and run it.

{rows}

Without arguments it opens board my-board as local-user, created on the
first run in WENA_DATABASE or the user's data folder; see --help. Each run
writes a debug log when WENA_LOG_DIR is set.

LICENSE is Wena's MIT license. NUKLEAR-LICENSE covers the included Nuklear
implementation. SDL2 (Zlib) and SQLite (public domain) are linked in; see
RELEASE-DEPENDENCIES.json for the exact, checksum-verified sources. The
embedded Roboto font is Apache-2.0: keep FONT-LICENSE.txt, FONT-PROVENANCE.json
and FONT-README.md. Translations come from WeKan under MIT; see
I18N-PROVENANCE.md.
"""


def notices(output, root=ROOT):
    files = {"README.txt": readme().encode("utf-8")}
    for name, source in NOTICES.items():
        files[name] = (Path(root) / source).read_bytes()
    raw = io.BytesIO()
    with tarfile.open(fileobj=raw, mode="w", format=tarfile.USTAR_FORMAT) as archive:
        for name in sorted(files):
            info = tarfile.TarInfo(f"wena-desktop-notices/{name}")
            info.size, info.mode, info.mtime, info.uid, info.gid = len(files[name]), 0o644, 0, 0, 0
            info.uname = info.gname = ""
            archive.addfile(info, io.BytesIO(files[name]))
    compressed = io.BytesIO()
    with gzip.GzipFile(fileobj=compressed, mode="wb", mtime=0, filename="") as stream:
        stream.write(raw.getvalue())
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    path = output / "wena-desktop-notices.tar.gz"
    path.write_bytes(compressed.getvalue())
    write_checksum(path)
    return path


def main(argv):
    try:
        if len(argv) == 4 and argv[0] == "binary":
            print(binary(*argv[1:]))
        elif len(argv) == 2 and argv[0] == "notices":
            print(notices(argv[1]))
        else:
            print("Usage: package_desktop_release.py binary TARGET EXECUTABLE OUTPUT_DIR | notices OUTPUT_DIR",
                  file=sys.stderr)
            return 2
    except (OSError, ValueError) as error:
        print(f"package_desktop_release: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
