#!/usr/bin/env python3
"""Check the native source boundaries that mirror Meteor WeKan."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    expected = (
        "client",
        "client/components",
        "client/features",
        "models",
        "imports",
        "server",
    )
    for relative in expected:
        directory = ROOT / relative
        assert directory.is_dir(), f"missing source boundary: {relative}"
        assert (directory / "README.md").is_file(), f"undocumented boundary: {relative}"

    assert (ROOT / "client/main.c").is_file()
    assert not (ROOT / "src/main.c").exists()
    for script in (ROOT / ".github/release").glob("*.sh"):
        text = script.read_text(encoding="utf-8")
        assert "src/main.c" not in text, f"stale source path in {script.name}"
        assert "client/main.c" in text, f"client entry point missing from {script.name}"


if __name__ == "__main__":
    main()
