#!/usr/bin/env python3
"""Unit tests for post-upload GitHub release asset verification."""

import hashlib
import json
from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
VERIFIER = ROOT / "scripts" / "verify_release_assets.py"


def run(local, response):
    return subprocess.run(
        ["python3", str(VERIFIER), "--local", str(local), "--response", str(response)],
        text=True, capture_output=True,
    )


def response_file(root, assets):
    path = root / "response.json"
    path.write_text(json.dumps(assets), encoding="utf-8")
    return path


def main():
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        local = root / "assets"
        local.mkdir()
        content = b"native executable\n"
        (local / "wena-linux-amd64").write_bytes(content)
        digest = "sha256:" + hashlib.sha256(content).hexdigest()
        response = response_file(root, [
            {"name": "unrelated-old-asset", "size": 1},
            {"name": "wena-linux-amd64", "size": len(content), "digest": digest},
        ])
        result = run(local, response)
        assert result.returncode == 0, result.stderr
        assert "verified 1 release assets" in result.stdout

        response = response_file(root, [])
        result = run(local, response)
        assert result.returncode != 0
        assert "missing remote asset" in result.stderr

        response = response_file(root, [
            {"name": "wena-linux-amd64", "size": len(content) + 1, "digest": digest},
        ])
        result = run(local, response)
        assert result.returncode != 0
        assert "size mismatch" in result.stderr

        response = response_file(root, [
            {"name": "wena-linux-amd64", "size": len(content), "digest": "sha256:bad"},
        ])
        result = run(local, response)
        assert result.returncode != 0
        assert "checksum mismatch" in result.stderr

        response = response_file(root, [
            {"name": "wena-linux-amd64", "size": len(content)},
            {"name": "wena-linux-amd64", "size": len(content)},
        ])
        result = run(local, response)
        assert result.returncode != 0
        assert "duplicate remote release asset" in result.stderr


if __name__ == "__main__":
    main()
