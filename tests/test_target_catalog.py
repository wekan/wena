#!/usr/bin/env python3
"""Validate the desktop release target catalog, config/targets.tsv."""

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "config" / "targets.tsv"
JOBS = {"linux", "bsd", "macos", "windows", "amiga", "android", "ios"}


def release_file(target):
    """wena-TARGET, with the extension its platform installs from."""
    system = target.split("-", 1)[0]
    return "wena-" + target + {"windows": ".exe", "android": ".apk", "ios": ".ipa"}.get(system, "")


def records():
    result = []
    for number, line in enumerate(CATALOG.read_text(encoding="utf-8").splitlines(), 1):
        if not line or line.startswith("#"):
            continue
        fields = line.split("\t")
        assert len(fields) == 5, f"targets.tsv:{number}: expected five fields"
        result.append(fields)
    return result


def main() -> None:
    entries = records()
    targets = [entry[0] for entry in entries]
    assert len(targets) == len(set(targets)), "duplicate target key"
    for target, name, job, status, release in entries:
        # system-cpu, and one display variant where a CPU has two: AmigaOS 3
        # with RTG or with AGA (amigaos-m68k-aga).
        assert re.fullmatch(r"[a-z0-9]+-[a-z0-9_]+(-aga)?", target), target
        assert not target.endswith("-aga") or target == "amigaos-m68k-aga", target
        assert name, target
        assert job in JOBS, (target, job)
        assert status in {"ready", "planned"}, (target, status)
        # Every release file is named after its target: wena-aros-amd64, wena-windows-amd64.exe.
        assert release == release_file(target), (target, release)
    # Every platform family the desktop is released for is in the catalog.
    systems = {target.split("-", 1)[0] for target in targets}
    assert {"linux", "freebsd", "netbsd", "openbsd", "dragonflybsd", "haiku", "macos", "windows",
            "amigaos", "amigaos4", "aros", "android", "ios"} <= systems, systems
    # The Linux CPUs Debian and Ubuntu still ship, including armel and mips64le.
    linux = {target for target in targets if target.startswith("linux-")}
    assert linux == {"linux-amd64", "linux-arm64", "linux-armhf", "linux-armel", "linux-i686",
                     "linux-riscv64", "linux-ppc64le", "linux-s390x", "linux-mips64le"}, linux
    # The CPU in a name is a CPU, never the ambiguous "x86" (wena-aros-x86 was
    # an x86-64 file); AROS has one target per CPU it is built for.
    assert not [t for t in targets if t.endswith("-x86")], targets
    assert {t for t in targets if t.startswith("aros-")} == {"aros-amd64", "aros-i386"}
    # Negative: no terminal-only program is released any more; every target is the GUI.
    text = CATALOG.read_text(encoding="utf-8")
    assert "wena-desktop-" not in text and "bootstrap" not in text
    assert not (ROOT / "client" / "main.c").exists()


if __name__ == "__main__":
    main()
