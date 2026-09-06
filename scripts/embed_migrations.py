#!/usr/bin/env python3
"""Append the pinned migration bytes and footer before the final i18n payload."""

import argparse
import hashlib
from pathlib import Path
import struct

from verify_migrations import verify


FOOTER_MAGIC = b"WENA-SQL-END-v1!"


def embed(executable):
    _lock, migration = verify()
    footer = hashlib.sha256(migration).digest() + struct.pack(">Q", len(migration)) + FOOTER_MAGIC
    with executable.open("ab") as destination:
        destination.write(migration)
        destination.write(footer)
    if executable.read_bytes()[-len(footer):] != footer:
        raise SystemExit(f"failed to verify embedded migration footer: {executable}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    args = parser.parse_args()
    if not args.executable.is_file():
        raise SystemExit(f"executable is missing: {args.executable}")
    embed(args.executable)
