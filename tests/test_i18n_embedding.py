#!/usr/bin/env python3
"""Verify every ready build embeds the pinned offline catalog."""

import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
FOOTER_MAGIC = b"WENA-I18N-END-v1"


def ready_targets():
    result = []
    for line in (ROOT / "config" / "targets.tsv").read_text(encoding="utf-8").splitlines():
        if line and not line.startswith("#"):
            fields = line.split("\t")
            if fields[3] == "ready":
                result.append(fields[0])
    return result


def embedded_catalog(executable):
    with executable.open("rb") as binary:
        binary.seek(-56, 2)
        footer = binary.read(56)
        assert footer[40:] == FOOTER_MAGIC
        size = struct.unpack(">Q", footer[32:40])[0]
        binary.seek(-56 - size, 2)
        catalog = binary.read(size)
    assert hashlib.sha256(catalog).digest() == footer[:32]
    return catalog


def main():
    for target in ready_targets():
        script = (ROOT / ".github" / "release" / f"{target}.sh").read_text(encoding="utf-8")
        assert "imports/i18n/catalog.c" in script, f"{target} omits strict-C89 reader"
        assert "embed_i18n_catalog.py" in script, f"{target} omits offline catalog"

    subprocess.run([str(ROOT / "build.sh"), "build", "host"], check=True)
    spec = importlib.util.spec_from_file_location("wena_commands", ROOT / "scripts" / "wena.py")
    wena = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(wena)
    target = wena.host_target()
    name = "wena.exe" if target.startswith("windows-") else "wena"
    executable = ROOT / "dist" / target / name
    lock = json.loads((ROOT / "config" / "i18n-lock.json").read_text(encoding="utf-8"))
    catalog = embedded_catalog(executable)
    assert hashlib.sha256(catalog).hexdigest() == lock["catalog_sha256"]
    result = subprocess.run([str(executable)], text=True, capture_output=True)
    assert result.returncode == 0, result.stderr
    assert result.stdout == "WeKan Native\n"

    with tempfile.TemporaryDirectory() as temporary:
        truncated = Path(temporary) / name
        shutil.copyfile(executable, truncated)
        truncated.chmod(0o755)
        with truncated.open("r+b") as damaged:
            damaged.seek(-1, 2)
            damaged.truncate()
        result = subprocess.run([str(truncated)], text=True, capture_output=True)
        assert result.returncode != 0
        assert "translation catalog is missing or invalid" in result.stderr


if __name__ == "__main__":
    main()
