#!/usr/bin/env python3
"""Real desktop extraction smoke plus deterministic and rejecting package checks."""

import importlib.util
from pathlib import Path
import struct
import subprocess
import tempfile
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("desktop_package", ROOT / "scripts/package_desktop.py")
packager = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packager)


def rejected(callback, message):
    try:
        callback()
    except ValueError as error:
        assert message in str(error), str(error)
    else:
        raise AssertionError("invalid package accepted")


def main():
    packager.host_check()
    with tempfile.TemporaryDirectory(prefix="wena-package-test-") as temporary:
        directory = Path(temporary)
        executable = directory / "wena-desktop"
        subprocess.run(["sh", str(ROOT / "scripts/build_desktop.sh"), str(executable)], check=True)
        one, two = directory / "one.tar.gz", directory / "two.tar.gz"
        metadata = packager.package(one, executable)
        assert packager.package(two, executable) == metadata
        assert one.read_bytes() == two.read_bytes(), "archive is nondeterministic"
        assert {"libSDL2-2.0.so.0", "libsqlite3.so.0"} <= set(metadata["dynamic_dependencies"])
        assert metadata["minimum_glibc"] and not metadata["bundled_system_libraries"]
        assert metadata["build_host_runtime"]["sqlite_source_id"]
        assert metadata["build_host_runtime"]["sqlite_runtime"].encode() in packager.package_files(executable)["README.txt"]
        assert packager.verify_archive(one) == metadata
        files = packager.package_files(executable)
        assert metadata["minimum_glibc"].encode() in files["README.txt"]
        assert "not claimed portable" in files["README.txt"].decode()
        assert files["LICENSE"] == (ROOT / "LICENSE").read_bytes()
        corrupted = dict(files, **{"wena-desktop": files["wena-desktop"][:-1] + b"!"})
        stage = packager.write_archive(directory / "corrupt", corrupted)
        rejected(lambda: packager.verify_archive(stage), "checksum mismatch")
        stage.unlink()
        incomplete = dict(files)
        del incomplete["LICENSE"]
        stage = packager.write_archive(directory / "incomplete", incomplete)
        rejected(lambda: packager.verify_archive(stage), "incomplete")
        stage.unlink()
        traversal = dict(files, **{"../outside": b"must not extract"})
        stage = packager.write_archive(directory / "traversal", traversal)
        rejected(lambda: packager.verify_archive(stage), "invalid package member")
        stage.unlink()
        assert not (directory / "outside").exists()
        bad = directory / "wrong-machine"
        data = bytearray(executable.read_bytes())
        struct.pack_into("<H", data, 18, 183)
        bad.write_bytes(data)
        rejected(lambda: packager.inspect_executable(bad), "Linux amd64 ELF")
        bad.write_bytes(executable.read_bytes()[:-1])
        rejected(lambda: packager.inspect_executable(bad), "missing embedded")
        with patch.object(packager.subprocess, "check_output", return_value=""):
            rejected(lambda: packager.inspect_executable(executable), "bootstrap is unsupported")
        before = one.read_bytes()
        with patch.object(packager, "verify_archive", side_effect=ValueError("smoke failed")):
            rejected(lambda: packager.package(one, executable), "smoke failed")
        assert one.read_bytes() == before, "failed verification replaced previous package"
        assert not list(directory.glob(".wena-package-*"))
        with patch.object(packager.platform, "machine", return_value="aarch64"):
            rejected(packager.host_check, "only on Linux amd64")
    print("Desktop package determinism, ELF/payload checks, no-clobber failure and extracted GUI smoke passed")


if __name__ == "__main__":
    main()
