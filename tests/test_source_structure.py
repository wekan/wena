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

    # The desktop is the one program; its entry point is client/desktop.c.
    assert (ROOT / "client/desktop.c").is_file()
    assert not (ROOT / "src/main.c").exists()
    # Negative: the terminal-only program that printed one line is gone.
    assert not (ROOT / "client/main.c").exists()
    assert "client/desktop.c" in (ROOT / "scripts/build_desktop.sh").read_text(encoding="utf-8")


if __name__ == "__main__":
    main()
