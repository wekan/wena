#!/usr/bin/env python3
"""Append the pinned offline catalog and a verifiable footer to an executable."""

import argparse
import hashlib
from pathlib import Path
import struct

from verify_i18n_catalog import ROOT, verify


FOOTER_MAGIC = b"WENA-I18N-END-v1"


def embed(executable):
    verify()
    lock_path = ROOT / "config" / "i18n-lock.json"
    import json
    lock = json.loads(lock_path.read_text(encoding="utf-8"))
    catalog = (ROOT / lock["catalog_path"]).read_bytes()
    footer = hashlib.sha256(catalog).digest() + struct.pack(">Q", len(catalog)) + FOOTER_MAGIC
    with executable.open("ab") as destination:
        destination.write(catalog)
        destination.write(footer)
    if executable.read_bytes()[-len(footer):] != footer:
        raise SystemExit(f"failed to verify embedded catalog footer: {executable}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    args = parser.parse_args()
    if not args.executable.is_file():
        raise SystemExit(f"executable is missing: {args.executable}")
    embed(args.executable)


if __name__ == "__main__":
    main()
