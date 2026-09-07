#!/usr/bin/env python3
"""Package and verify the real Linux amd64 desktop, separate from bootstrap targets."""

import argparse
import gzip
import hashlib
import io
import json
import os
from pathlib import Path
import platform
import re
import shutil
import sqlite3
import struct
import subprocess
import tarfile
import tempfile
from urllib.parse import unquote

ROOT = Path(__file__).resolve().parents[1]
PREFIX = "wena-desktop-linux-amd64"


def digest(data):
    return hashlib.sha256(data).hexdigest()


def host_check():
    if platform.system() != "Linux" or platform.machine().lower() not in {"x86_64", "amd64"}:
        raise ValueError("desktop packaging is verified only on Linux amd64")
    if not shutil.which("readelf"):
        raise ValueError("desktop packaging requires readelf (binutils)")


def embedded(data, end, magic):
    if end < 56 or data[end - 16:end] != magic:
        raise ValueError("missing embedded " + magic.decode("ascii") + " footer")
    footer = data[end - 56:end]
    length = struct.unpack(">Q", footer[32:40])[0]
    start = end - 56 - length
    if start < 64:
        raise ValueError("invalid embedded payload length")
    payload = data[start:end - 56]
    if hashlib.sha256(payload).digest() != footer[:32]:
        raise ValueError("embedded payload checksum mismatch")
    return start, payload


def inspect_executable(executable):
    data = executable.read_bytes()
    if (len(data) < 64 or data[:6] != b"\x7fELF\x02\x01" or
            struct.unpack_from("<H", data, 18)[0] != 62 or
            struct.unpack_from("<H", data, 16)[0] not in {2, 3}):
        raise ValueError("expected Linux amd64 ELF executable")
    end, catalog = embedded(data, len(data), b"WENA-I18N-END-v1")
    _start, migration = embedded(data, end, b"WENA-SQL-END-v1!")
    i18n = json.loads((ROOT / "config/i18n-lock.json").read_text())
    sql = json.loads((ROOT / "config/migrations-lock.json").read_text())
    dependencies = json.loads((ROOT / "config/dependencies-lock.json").read_text())
    font = json.loads((ROOT / "third_party/fonts/provenance.json").read_text())
    if sql.get("format") == 1:
        migration_hash, migration_size = sql["migration_sha256"], sql["migration_size"]
    elif sql.get("format") == 2 and sql.get("migrations"):
        final = sql["migrations"][-1]
        migration_hash, migration_size = final["bundle_sha256"], final["bundle_size"]
    else:
        raise ValueError("unsupported migration lock format")
    if (digest(catalog) != i18n["catalog_sha256"] or
            digest(migration) != migration_hash or len(migration) != migration_size):
        raise ValueError("embedded payload differs from pinned source locks")
    env = dict(os.environ, LC_ALL="C")
    details = subprocess.check_output(
        ["readelf", "--wide", "--dynamic", "--version-info", str(executable)],
        text=True, env=env)
    needed = sorted(set(re.findall(r"Shared library: \[([^\]]+)\]", details)))
    if not {"libSDL2-2.0.so.0", "libsqlite3.so.0"}.issubset(needed):
        raise ValueError("artifact is not the SDL2/SQLite desktop (bootstrap is unsupported)")
    versions = set(re.findall(r"Name: GLIBC_([0-9]+(?:\.[0-9]+)+)", details))
    if not versions:
        raise ValueError("could not determine required glibc versions from ELF")
    minimum = max(versions, key=lambda value: tuple(map(int, value.split("."))))
    return {"format": 1, "artifact": "wena-desktop", "target": "linux-amd64",
            "sha256": digest(data), "bytes": len(data), "minimum_glibc": minimum,
            "dynamic_dependencies": needed, "bundled_system_libraries": False,
            "i18n": i18n, "migration": sql, "dependencies": dependencies, "font": font}


def executable_runtime(executable):
    env = dict(os.environ, SDL_VIDEODRIVER="wena-invalid-driver")
    result = subprocess.run([str(executable), "--dependency-info"], env=env,
                            text=True, capture_output=True, timeout=10)
    if result.returncode or len(result.stdout) > 16384:
        raise ValueError("executable dependency report failed: " + result.stderr)
    report = {}
    for line in result.stdout.splitlines():
        key, separator, value = line.partition("=")
        if not separator or key in report:
            raise ValueError("invalid executable dependency report")
        report[key] = unquote(value, errors="strict")
    if (report.get("format") != "wena-dependencies-v1" or
            report.get("scope") != "libraries_loaded_by_this_process" or
            not report.get("sqlite_source_id")):
        raise ValueError("missing executable runtime provenance")
    for field in ("sqlite_runtime_number", "sqlite_threadsafe"):
        report[field] = int(report[field])
    report["sqlite_wal_reset_known_fixed_upstream_version"] = (
        report["sqlite_wal_reset_known_fixed_upstream_version"] == "1")
    report["evidence"] = "packaged executable --dependency-info on the build host"
    return report


def package_files(executable):
    metadata = inspect_executable(executable)
    metadata["build_host_runtime"] = executable_runtime(executable)
    requirements = ", ".join(metadata["dynamic_dependencies"])
    readme = f"""Wena desktop for Linux amd64
===========================

This package contains the actual SDL2/SQLite local desktop application.
It is separate from Wena's cataloged bootstrap release artifacts.

Runtime requirements: Linux x86-64, glibc {metadata['minimum_glibc']} or newer,
and compatible system shared libraries: {requirements}.
The glibc requirement is read from this executable's ELF version requirements.
SDL2 >= {metadata['dependencies']['system_dependencies']['SDL2']['minimum_backend_version']} and SQLite must be installed by your operating system. System libraries
are not bundled. This package is not claimed portable to every Linux system.
A graphical SDL2 video driver is required for interactive use.
Build-host probe: SDL2 {metadata['build_host_runtime']['sdl_runtime']},
SQLite {metadata['build_host_runtime']['sqlite_runtime']}; source ID:
{metadata['build_host_runtime']['sqlite_source_id']}
This records the verification host, not libraries installed on another machine.

Create a local workspace (use an absolute database path):
  ./wena-desktop --database /absolute/path/board.sqlite --actor local --board board --create
Open it later:
  ./wena-desktop --database /absolute/path/board.sqlite --actor local --board board
Optional flags: --title TITLE with --create; --language LOCALE; --smoke.
Creation refuses to overwrite an existing workspace. Remote mode is not included.

The executable embeds the pinned SQLite migration and all canonical WeKan
translations. manifest.json records their provenance, hashes, and runtime needs.
SHA256SUMS covers every other package file. LICENSE is Wena's MIT license;
NUKLEAR-LICENSE covers the included Nuklear implementation.
The embedded Roboto font is Apache-2.0; retain FONT-LICENSE.txt,
FONT-PROVENANCE.json and FONT-README.md. No runtime font file is required. Translations come
from WeKan under MIT; see I18N-PROVENANCE.md and the pinned manifest revision.
SDL2 (zlib license) and SQLite (public domain) remain system dependencies.
DEPENDENCIES.json pins compiled inputs and records system-library requirements;
DEPENDENCY-AUDIT.md documents provenance and deployment caveats. Host probes do
not establish the dependency versions installed on another machine.

Archive metadata is deterministic for identical executable and source metadata.
This does not claim reproducible binaries across compilers or build directories.
"""
    files = {"wena-desktop": executable.read_bytes(),
             "README.txt": readme.encode("utf-8"),
             "manifest.json": (json.dumps(metadata, indent=2, sort_keys=True) + "\n").encode(),
             "LICENSE": (ROOT / "LICENSE").read_bytes(),
             "NUKLEAR-LICENSE": (ROOT / "third_party/nuklear/LICENSE").read_bytes(),
             "I18N-PROVENANCE.md": (ROOT / "imports/i18n/README.md").read_bytes(),
             "DEPENDENCIES.json": (ROOT / "config/dependencies-lock.json").read_bytes(),
             "DEPENDENCY-AUDIT.md": (ROOT / "docs/dependency-audit.md").read_bytes(),
             "FONT-LICENSE.txt": (ROOT / "third_party/fonts/LICENSE-Roboto.txt").read_bytes(),
             "FONT-PROVENANCE.json": (ROOT / "third_party/fonts/provenance.json").read_bytes(),
             "FONT-README.md": (ROOT / "third_party/fonts/README.md").read_bytes()}
    files["SHA256SUMS"] = "".join(
        f"{digest(content)}  {name}\n" for name, content in sorted(files.items())).encode()
    return files


def write_archive(output, files):
    # Stage next to the destination so a failed build never replaces an artifact.
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=output.parent, prefix=".wena-package-", delete=False) as stage:
        stage_path = Path(stage.name)
    try:
        with stage_path.open("wb") as raw:
            with gzip.GzipFile(filename="", fileobj=raw, mode="wb", mtime=0) as compressed:
                with tarfile.open(fileobj=compressed, mode="w", format=tarfile.USTAR_FORMAT) as archive:
                    for name, content in sorted(files.items()):
                        info = tarfile.TarInfo(PREFIX + "/" + name)
                        info.size = len(content)
                        info.mode = 0o755 if name == "wena-desktop" else 0o644
                        info.mtime = 0
                        archive.addfile(info, io.BytesIO(content))
        return stage_path
    except BaseException:
        stage_path.unlink(missing_ok=True)
        raise


def verify_archive(archive_path, smoke=True):
    expected = {"wena-desktop", "README.txt", "manifest.json", "LICENSE",
                "NUKLEAR-LICENSE", "I18N-PROVENANCE.md", "DEPENDENCIES.json",
                "DEPENDENCY-AUDIT.md", "FONT-LICENSE.txt", "FONT-PROVENANCE.json",
                "FONT-README.md", "SHA256SUMS"}
    files = {}
    with tarfile.open(archive_path, "r:gz") as archive:
        for member in archive:
            name = member.name.removeprefix(PREFIX + "/")
            if (member.name != PREFIX + "/" + name or name not in expected or
                    name in files or not member.isfile() or member.size > 64 * 1024 * 1024):
                raise ValueError("invalid package member")
            if member.mode != (0o755 if name == "wena-desktop" else 0o644):
                raise ValueError("invalid package permissions")
            files[name] = archive.extractfile(member).read()
    if set(files) != expected:
        raise ValueError("incomplete desktop package")
    sums = "".join(f"{digest(content)}  {name}\n" for name, content in sorted(files.items())
                   if name != "SHA256SUMS").encode()
    if files["SHA256SUMS"] != sums:
        raise ValueError("package checksum mismatch")
    metadata = json.loads(files["manifest.json"])
    if metadata["sha256"] != digest(files["wena-desktop"]):
        raise ValueError("manifest executable checksum mismatch")
    # Write only allowlisted flat members; never extract archive-supplied paths.
    with tempfile.TemporaryDirectory(prefix="wena extracted desktop ") as temporary:
        directory = Path(temporary)
        for name, content in files.items():
            (directory / name).write_bytes(content)
        executable = directory / "wena-desktop"
        executable.chmod(0o755)
        host = metadata.get("build_host_runtime")
        if not isinstance(host, dict) or not host.get("sqlite_source_id"):
            raise ValueError("missing build-host runtime provenance")
        inspected = dict(metadata)
        del inspected["build_host_runtime"]
        if inspect_executable(executable) != inspected:
            raise ValueError("manifest differs from inspected executable")
        if smoke:
            database = directory / "workspace.sqlite"
            env = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy")
            result = subprocess.run([str(executable), "--database", str(database),
                "--actor", "package-test", "--board", "package-board", "--create", "--smoke"],
                env=env, text=True, capture_output=True, timeout=30)
            if result.returncode or "Wena desktop smoke passed" not in result.stdout or not database.is_file():
                raise ValueError("extracted desktop smoke failed: " + result.stderr)
            with sqlite3.connect(database) as connection:
                if (connection.execute("PRAGMA user_version").fetchone() !=
                        (metadata["migration"]["schema_version"],) or
                        connection.execute("PRAGMA integrity_check").fetchone() != ("ok",) or
                        connection.execute("PRAGMA foreign_key_check").fetchall()):
                    raise ValueError("extracted workspace schema/integrity check failed")
    return metadata


def package(output, executable=None):
    host_check()
    with tempfile.TemporaryDirectory(prefix="wena-desktop-build-") as temporary:
        if executable is None:
            executable = Path(temporary) / "wena-desktop"
            subprocess.run(["sh", str(ROOT / "scripts/build_desktop.sh"), str(executable)], check=True)
        stage = write_archive(output, package_files(executable))
        try:
            metadata = verify_archive(stage)
            os.replace(stage, output)
        finally:
            stage.unlink(missing_ok=True)
    return metadata


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "dist/desktop" / (PREFIX + ".tar.gz"))
    args = parser.parse_args()
    try:
        metadata = package(args.output)
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        parser.exit(1, f"desktop package failed: {error}\n")
    print(f"Verified desktop package: {args.output} (glibc >= {metadata['minimum_glibc']})")


if __name__ == "__main__":
    main()
