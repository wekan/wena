#!/usr/bin/env python3
"""Positive and corruption checks for the pinned offline catalog."""

import json
from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
VERIFY = ROOT / "scripts" / "verify_i18n_catalog.py"
LOCK = ROOT / "config" / "i18n-lock.json"


def run(lock=LOCK):
    return subprocess.run(["python3", str(VERIFY), str(lock)], text=True, capture_output=True)


def main():
    valid = run()
    assert valid.returncode == 0, valid.stderr
    assert "246 languages" in valid.stdout

    with tempfile.TemporaryDirectory(dir=ROOT) as temporary:
        temp = Path(temporary)
        catalog = temp / "catalog.bin"
        source = ROOT / "imports" / "i18n" / "wekan-i18n.bin"
        catalog.write_bytes(source.read_bytes()[:-1] + b"x")
        lock = json.loads(LOCK.read_text(encoding="utf-8"))
        lock["catalog_path"] = str(catalog.relative_to(ROOT))
        lock_path = temp / "lock.json"
        lock_path.write_text(json.dumps(lock), encoding="utf-8")
        corrupt = run(lock_path)
        assert corrupt.returncode != 0
        assert "SHA-256 differs from lock" in corrupt.stderr

        lock["catalog_path"] = str((temp / "missing.bin").relative_to(ROOT))
        lock_path.write_text(json.dumps(lock), encoding="utf-8")
        missing = run(lock_path)
        assert missing.returncode != 0
        assert "missing" in missing.stderr


if __name__ == "__main__":
    main()
