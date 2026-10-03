#!/usr/bin/env python3
"""Package one platform's desktop build for a release, deterministically.

Used by .github/workflows/release-desktop.yml after `wena.py build desktop`:

    package_desktop_release.py TARGET BINARY OUTPUT_DIR

writes OUTPUT_DIR/wena-desktop-TARGET.tar.gz, holding wena-desktop-TARGET/ with
the executable, its licenses and provenance, a README naming the system
libraries it needs, and SHA256SUMS; and OUTPUT_DIR/wena-desktop-TARGET.tar.gz.sha256
in the format `sha256sum -c` reads. The same inputs give the same bytes.
"""

import gzip
import hashlib
import io
from pathlib import Path
import re
import sys
import tarfile

ROOT = Path(__file__).resolve().parents[1]
TARGETS = {
    "linux-amd64": ("Linux amd64", "sudo apt install libsdl2-2.0-0 libsqlite3-0"),
    "linux-arm64": ("Linux arm64", "sudo apt install libsdl2-2.0-0 libsqlite3-0"),
    "macos-arm64": ("macOS arm64", "brew install sdl2   (SQLite ships with macOS)"),
    "macos-amd64": ("macOS amd64", "brew install sdl2   (SQLite ships with macOS)"),
}
FILES = {
    "LICENSE": "LICENSE",
    "NUKLEAR-LICENSE": "third_party/nuklear/LICENSE",
    "I18N-PROVENANCE.md": "imports/i18n/README.md",
    "DEPENDENCIES.json": "config/dependencies-lock.json",
    "DEPENDENCY-AUDIT.md": "docs/dependency-audit.md",
    "FONT-LICENSE.txt": "third_party/fonts/LICENSE-Roboto.txt",
    "FONT-PROVENANCE.json": "third_party/fonts/provenance.json",
    "FONT-README.md": "third_party/fonts/README.md",
}


def readme(target):
    name, install = TARGETS[target]
    return f"""Wena desktop for {name}

Run ./wena-desktop. Without arguments it opens board my-board as local-user,
created on the first run in WENA_DATABASE or the user's data folder; see
./wena-desktop --help. Each run writes a debug log when WENA_LOG_DIR is set.

SDL2 and SQLite are system libraries, not included:
  {install}

SHA256SUMS covers every other file here. LICENSE is Wena's MIT license;
NUKLEAR-LICENSE covers the included Nuklear implementation. The embedded
Roboto font is Apache-2.0; keep FONT-LICENSE.txt, FONT-PROVENANCE.json and
FONT-README.md. Translations come from WeKan under MIT; see I18N-PROVENANCE.md.
"""


def entry(archive, name, data, mode):
    info = tarfile.TarInfo(name)
    info.size, info.mode, info.mtime = len(data), mode, 0
    info.uid = info.gid = 0
    info.uname = info.gname = ""
    archive.addfile(info, io.BytesIO(data))


def package(target, binary, output, root=ROOT):
    if target not in TARGETS:
        raise ValueError(f"unknown desktop release target: {target}")
    binary = Path(binary)
    data = binary.read_bytes()
    if not data:
        raise ValueError(f"empty desktop executable: {binary}")
    prefix = f"wena-desktop-{target}"
    files = {"wena-desktop": data, "README.txt": readme(target).encode("utf-8")}
    for name, source in FILES.items():
        files[name] = (Path(root) / source).read_bytes()
    sums = "".join(f"{hashlib.sha256(files[name]).hexdigest()}  {name}\n" for name in sorted(files))
    files["SHA256SUMS"] = sums.encode("ascii")
    raw = io.BytesIO()
    with tarfile.open(fileobj=raw, mode="w", format=tarfile.USTAR_FORMAT) as archive:
        directory = tarfile.TarInfo(prefix)
        directory.type, directory.mode, directory.mtime = tarfile.DIRTYPE, 0o755, 0
        archive.addfile(directory)
        for name in sorted(files):
            entry(archive, f"{prefix}/{name}", files[name], 0o755 if name == "wena-desktop" else 0o644)
    compressed = io.BytesIO()
    with gzip.GzipFile(fileobj=compressed, mode="wb", mtime=0, filename="") as stream:
        stream.write(raw.getvalue())
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    archive_path = output / f"{prefix}.tar.gz"
    archive_path.write_bytes(compressed.getvalue())
    digest = hashlib.sha256(compressed.getvalue()).hexdigest()
    (output / f"{prefix}.tar.gz.sha256").write_text(f"{digest}  {prefix}.tar.gz\n", encoding="ascii")
    return archive_path


def main(argv):
    if len(argv) != 3 or not re.fullmatch(r"[a-z0-9-]+", argv[0]):
        print("Usage: package_desktop_release.py TARGET BINARY OUTPUT_DIR", file=sys.stderr)
        return 2
    try:
        print(package(*argv))
    except (OSError, ValueError) as error:
        print(f"package_desktop_release: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
