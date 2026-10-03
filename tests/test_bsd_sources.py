#!/usr/bin/env python3
"""Every desktop source compiles for FreeBSD, OpenBSD and NetBSD, here.

The BSD release builds run in virtual machines on GitHub. Their compile errors
are cheaper to find here: clang compiles each source of scripts/build_desktop.sh
with the release flags against each system's own headers, from its official
release sets (pinned below, the digests from each project's checksum file).
It found two: an unused static helper on FreeBSD and NetBSD, and NetBSD's
<sys/sysctl.h> needing _NETBSD_SOURCE beside _POSIX_C_SOURCE.
"""

import importlib.util
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
CACHE = ROOT / ".tools" / "cache"
SYSTEMS = {
    # name: (clang target, pin of the set holding usr/include)
    "freebsd": ("x86_64-unknown-freebsd15.1", {
        "url": "https://download.freebsd.org/releases/amd64/15.1-RELEASE/base.txz",
        "sha256": "3768988b151c20f965679062b065c63a977d6bbb9f47fd83695ec2c40790c18f"}),
    "openbsd": ("x86_64-unknown-openbsd7.9", {
        "url": "https://cdn.openbsd.org/pub/OpenBSD/7.9/amd64/comp79.tgz",
        "sha256": "21a67af20aebcabf85b09f4206fc95b4cae0a35d42b154b976f0159f457724f9"}),
    # NetBSD publishes SHA-512 (ea90d527d1475bd6...); this is the same file's SHA-256.
    "netbsd": ("x86_64-unknown-netbsd10.1", {
        "url": "https://cdn.netbsd.org/pub/NetBSD/NetBSD-10.1/amd64/binary/sets/comp.tar.xz",
        "sha256": "02e51e63e05b54f9d30d4d566c55e96e1b8b36fb8e870b1cc3ed9494d93d11f3"}),
}
FLAGS = ["-std=c89", "-pedantic-errors", "-Wall", "-Wextra", "-Werror", "-DNK_INPUT_MAX=256"]


def fetcher():
    spec = importlib.util.spec_from_file_location("fetch", ROOT / "scripts" / "fetch_release_dependency.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def headers(name, pin, fetch):
    """The system's usr/include, unpacked once into the cache."""
    root = CACHE / "sysroots" / name
    if (root / ".complete").is_file():
        return root
    archive = fetch.fetch(name, CACHE, pins={name: pin})
    shutil.rmtree(root, ignore_errors=True)
    with tarfile.open(archive) as bundle:
        members = [m for m in bundle.getmembers()
                   if m.name.lstrip("./").startswith("usr/include/") and (m.isfile() or m.isdir() or m.issym())]
        for member in members:
            member.name = member.name.lstrip("./")
            if member.issym() and (member.linkname.startswith("/") or ".." in member.linkname.split("/")):
                continue
            bundle.extract(member, root)
    # NetBSD's <machine/...> is a symlink that ships in the base set, not comp.
    machine = root / "usr" / "include" / "machine"
    if name == "netbsd" and not machine.exists():
        machine.symlink_to("amd64")
    (root / ".complete").write_text("ok\n")
    return root


def sources():
    script = (ROOT / "scripts" / "build_desktop.sh").read_text(encoding="utf-8")
    return sorted(set(re.findall(r'\$root_dir/([^" ]+\.c)', script[script.index("$WENA_CC "):])))


def compile_all(target, sysroot, includes, files):
    failed = {}
    for source in files:
        result = subprocess.run(["clang", f"--target={target}", f"--sysroot={sysroot}", *FLAGS,
                                 *includes, "-fsyntax-only", str(ROOT / source)],
                                capture_output=True, text=True)
        if result.returncode:
            failed[source] = result.stderr
    return failed


def main():
    if not shutil.which("clang"):
        print("SKIP: needs clang")
        return 0
    fetch = fetcher()
    pins = __import__("json").loads((ROOT / "config" / "release-dependencies.json").read_text())
    work = Path(tempfile.mkdtemp(dir=CACHE.parent if CACHE.parent.is_dir() else None))
    try:
        sdl = fetch.fetch("sdl2", CACHE, pins=pins)
        with tarfile.open(sdl) as bundle:
            bundle.extractall(work, [m for m in bundle.getmembers() if "/include/" in m.name])
        sqlite = fetch.fetch("sqlite", CACHE, pins=pins)
        with zipfile.ZipFile(sqlite) as bundle:
            name = next(n for n in bundle.namelist() if n.endswith("/sqlite3.h"))
            (work / "sqlite3.h").write_bytes(bundle.read(name))
        includes = ["-I" + str(ROOT / "third_party" / "nuklear"),
                    "-I" + str(next(work.glob("SDL2-*/include"))), "-isystem", str(work)]
        files = sources()
        assert len(files) > 90, len(files)
        problems = 0
        for name, (target, pin) in SYSTEMS.items():
            sysroot = headers(name, pin, fetch)
            failed = compile_all(target, sysroot, includes, files)
            print(f"{name}: {len(files) - len(failed)} of {len(files)} sources compile")
            for source, error in failed.items():
                print(f"--- {source}\n{error}", file=sys.stderr)
            problems += len(failed)
        # Negative: the check itself fails on what broke FreeBSD - an unused static helper.
        probe = work / "probe.c"
        probe.write_text("static int unused(void) { return 0; }\nint main(void) { return 0; }\n")
        failed = compile_all(SYSTEMS["freebsd"][0], headers("freebsd", SYSTEMS["freebsd"][1], fetch), [], [str(probe)])
        assert failed, "an unused static function was accepted"
        return 1 if problems else 0
    finally:
        shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
