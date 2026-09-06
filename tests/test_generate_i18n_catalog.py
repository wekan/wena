#!/usr/bin/env python3
"""Fail-closed tests for deterministic canonical WeKan i18n generation."""

import json
from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
GENERATOR = ROOT / "scripts" / "generate_i18n_catalog.py"


def write(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False), encoding="utf-8")


def run(source, output, check=False):
    arguments = ["python3", str(GENERATOR), "--source", str(source),
                 "--source-revision", "wekan-test-revision", "--output", str(output)]
    if check:
        arguments.append("--check")
    return subprocess.run(arguments, text=True, capture_output=True)


def main():
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        source = root / "data"
        source.mkdir()
        english = {"hello": "Hello __username__ %s", "direction": "Left"}
        write(source / "en.i18n.json", english)
        write(source / "ar.i18n.json", {"hello": "مرحبا __username__ %s", "direction": "يسار"})
        output = root / "catalog.bin"
        first = run(source, output)
        assert first.returncode == 0, first.stderr
        original = output.read_bytes()
        assert original.startswith(b"WENA-I18N-1\n")
        assert run(source, output, check=True).returncode == 0
        output.write_bytes(original + b"stale")
        stale = run(source, output, check=True)
        assert stale.returncode != 0
        assert "stale or incomplete" in stale.stderr
        run(source, output)
        assert output.read_bytes() == original, "generation must be deterministic"

        write(source / "ar.i18n.json", {"direction": "يسار", "hello": "مرحبا __username__ %s"})
        order = run(source, output)
        assert order.returncode != 0
        assert "key/order mismatch" in order.stderr
        write(source / "ar.i18n.json", {"hello": "مرحبا %s", "direction": "يسار"})
        placeholder = run(source, output)
        assert placeholder.returncode != 0
        assert "placeholder inventory differs" in placeholder.stderr
        write(source / "ar.i18n.json", {"hello": "مرحبا __username__ %s", "direction": "يسار", "extra": "x"})
        extra = run(source, output)
        assert extra.returncode != 0
        assert "key/order mismatch" in extra.stderr


if __name__ == "__main__":
    main()
