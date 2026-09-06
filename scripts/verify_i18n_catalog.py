#!/usr/bin/env python3
"""Verify the pinned offline i18n catalog before every target build."""

import hashlib
import json
from pathlib import Path
import struct
import sys
import zlib


ROOT = Path(__file__).resolve().parents[1]
MAGIC = b"WENA-I18N-1\n"


def fail(message):
    raise SystemExit("i18n catalog verification failed: " + message)


def verify(lock_path=None):
    lock_path = lock_path or ROOT / "config" / "i18n-lock.json"
    lock = json.loads(lock_path.read_text(encoding="utf-8"))
    catalog = ROOT / lock["catalog_path"]
    if not catalog.is_file():
        fail(f"missing {catalog.relative_to(ROOT)}")
    raw = catalog.read_bytes()
    if len(raw) > lock["maximum_catalog_bytes"]:
        fail("catalog exceeds maximum_catalog_bytes")
    if len(raw) != lock["catalog_bytes"]:
        fail("catalog byte count differs from lock")
    if hashlib.sha256(raw).hexdigest() != lock["catalog_sha256"]:
        fail("catalog SHA-256 differs from lock")
    if not raw.startswith(MAGIC) or len(raw) < len(MAGIC) + 8:
        fail("catalog marker is missing")
    uncompressed_size, header_languages = struct.unpack(">II", raw[len(MAGIC):len(MAGIC) + 8])
    if uncompressed_size > 64 * 1024 * 1024:
        fail("uncompressed catalog exceeds 64 MiB safety limit")
    try:
        payload_bytes = zlib.decompress(raw[len(MAGIC) + 8:])
        payload = json.loads(payload_bytes.decode("utf-8"))
    except (zlib.error, UnicodeDecodeError, json.JSONDecodeError) as error:
        fail(f"catalog payload is invalid: {error}")
    if len(payload_bytes) != uncompressed_size:
        fail("uncompressed byte count differs from header")
    languages = payload.get("languages", [])
    tags = [language.get("tag") for language in languages]
    if len(tags) != lock["language_count"] or header_languages != len(tags):
        fail("language count differs from lock/header")
    if len(tags) != len(set(tags)) or "en" not in tags:
        fail("language tags are duplicate or English is missing")
    if payload.get("source_revision") != lock["source_revision"]:
        fail("source revision differs from lock")
    if payload.get("english_key_count") != lock["english_key_count"]:
        fail("English key count differs from lock")
    if len(payload.get("sources", [])) != len(tags):
        fail("source hash inventory is incomplete")
    for language in languages:
        if len(language.get("entries", [])) != lock["english_key_count"]:
            fail(f"{language.get('tag')}: key inventory is incomplete")
    return len(tags)


if __name__ == "__main__":
    count = verify(Path(sys.argv[1]) if len(sys.argv) == 2 else None)
    print(f"verified pinned offline i18n catalog: {count} languages")
