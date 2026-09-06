#!/usr/bin/env python3
"""Positive and fail-closed tests for release asset collection."""

from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
COLLECTOR = ROOT / "scripts" / "collect_release_assets.py"


def run(catalog, incoming, output):
    return subprocess.run(
        ["python3", str(COLLECTOR), "--catalog", str(catalog),
         "--incoming", str(incoming), "--output", str(output),
         "--repository", "wekan/wena", "--commit", "a" * 40],
        text=True, capture_output=True,
    )


def fixture(root):
    catalog = root / "targets.tsv"
    catalog.write_text(
        "linux-amd64\tLinux amd64\tubuntu-24.04\tready\tELF executable\n"
        "windows-amd64\tWindows amd64\tubuntu-24.04\tready\tPE executable\n"
        "linux-i686\tLinux i686\tubuntu-24.04\tplanned\tELF executable\n",
        encoding="utf-8",
    )
    incoming = root / "incoming"
    (incoming / "wena-linux-amd64").mkdir(parents=True)
    (incoming / "wena-linux-amd64" / "wena").write_bytes(b"linux\n")
    (incoming / "wena-windows-amd64").mkdir()
    (incoming / "wena-windows-amd64" / "wena.exe").write_bytes(b"windows\n")
    return catalog, incoming


def main():
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        catalog, incoming = fixture(root)
        result = run(catalog, incoming, root / "assets")
        assert result.returncode == 0, result.stderr
        assets = root / "assets"
        assert sorted(item.name for item in assets.iterdir()) == [
            "MANIFEST.tsv", "SHA256SUMS", "wena-linux-amd64", "wena-windows-amd64.exe"
        ]
        manifest = (assets / "MANIFEST.tsv").read_text(encoding="utf-8")
        assert "wekan/wena\t" + "a" * 40 in manifest
        checksums = (assets / "SHA256SUMS").read_text(encoding="utf-8")
        assert "  wena-linux-amd64\n" in checksums
        assert "  wena-windows-amd64.exe\n" in checksums

    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        catalog, incoming = fixture(root)
        (incoming / "wena-linux-amd64" / "unexpected.txt").write_text("no", encoding="utf-8")
        result = run(catalog, incoming, root / "assets")
        assert result.returncode != 0
        assert "expected only wena" in result.stderr

    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        catalog, incoming = fixture(root)
        (incoming / "wena-windows-amd64" / "wena.exe").unlink()
        result = run(catalog, incoming, root / "assets")
        assert result.returncode != 0
        assert "expected only wena.exe; found none" in result.stderr

    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        catalog, incoming = fixture(root)
        (incoming / "unrequested-artifact").mkdir()
        result = run(catalog, incoming, root / "assets")
        assert result.returncode != 0
        assert "unexpected Actions artifacts: unrequested-artifact" in result.stderr

    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        catalog, incoming = fixture(root)
        with catalog.open("a", encoding="utf-8") as target_catalog:
            target_catalog.write("linux-amd64\tDuplicate\tubuntu-24.04\tready\tELF executable\n")
        result = run(catalog, incoming, root / "assets")
        assert result.returncode != 0
        assert "duplicate ready target" in result.stderr


if __name__ == "__main__":
    main()
