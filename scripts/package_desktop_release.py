#!/usr/bin/env python3
"""Release files for the desktop: one file per platform, and their checksums.

    package_desktop_release.py binary TARGET FILE OUTPUT_DIR
        -> OUTPUT_DIR/<release file of TARGET in config/targets.tsv>
    package_desktop_release.py sums OUTPUT_DIR
        -> OUTPUT_DIR/SHA256SUMS, for every release file in it
    package_desktop_release.py missing OUTPUT_DIR
        -> the ready targets whose release file is not in OUTPUT_DIR

Each platform's file - wena-linux-amd64, wena-windows-amd64.exe,
wena-android-arm64.apk ... - is attached on its own, so a user downloads only
theirs. Everything it needs is inside it, including the licenses of what is
linked in (`wena --licenses`). SHA256SUMS is in the format `sha256sum -c`
reads.
"""

import hashlib
from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "config" / "targets.tsv"


def targets(catalog=CATALOG):
    """target -> (display name, job, status, release file)."""
    result = {}
    for number, line in enumerate(Path(catalog).read_text(encoding="utf-8").splitlines(), 1):
        if not line or line.startswith("#"):
            continue
        fields = line.split("\t")
        if len(fields) != 5:
            raise ValueError(f"targets.tsv:{number}: expected five fields")
        result[fields[0]] = tuple(fields[1:])
    return result


def release_name(target, catalog=CATALOG):
    record = targets(catalog).get(target)
    if record is None:
        raise ValueError(f"unknown desktop release target: {target}")
    return record[3]


def binary(target, executable, output, catalog=CATALOG):
    name = release_name(target, catalog)
    executable = Path(executable)
    if not executable.is_file() or executable.stat().st_size == 0:
        raise ValueError(f"missing or empty release file: {executable}")
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    path = output / name
    shutil.copyfile(executable, path)
    path.chmod(0o755)
    return path


def sums(output, catalog=CATALOG):
    output = Path(output)
    names = {record[3] for record in targets(catalog).values()}
    present = sorted(path.name for path in output.iterdir() if path.name in names)
    unknown = sorted(path.name for path in output.iterdir()
                     if path.is_file() and path.name not in names and path.name != "SHA256SUMS")
    if unknown:
        raise ValueError("not a release file of any target: " + ", ".join(unknown))
    if not present:
        raise ValueError(f"no release files in {output}")
    lines = [f"{hashlib.sha256((output / name).read_bytes()).hexdigest()}  {name}\n" for name in present]
    path = output / "SHA256SUMS"
    path.write_text("".join(lines), encoding="ascii")
    return path


def missing(output, catalog=CATALOG):
    output = Path(output)
    return [target for target, record in targets(catalog).items()
            if record[2] == "ready" and not (output / record[3]).is_file()]


def main(argv):
    try:
        if len(argv) == 4 and argv[0] == "binary":
            print(binary(*argv[1:]))
        elif len(argv) == 2 and argv[0] == "sums":
            print(sums(argv[1]))
        elif len(argv) == 2 and argv[0] == "missing":
            print(" ".join(missing(argv[1])))
        else:
            print("Usage: package_desktop_release.py binary TARGET FILE OUTPUT_DIR | sums OUTPUT_DIR | missing OUTPUT_DIR",
                  file=sys.stderr)
            return 2
    except (OSError, ValueError) as error:
        print(f"package_desktop_release: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
