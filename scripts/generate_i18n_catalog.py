#!/usr/bin/env python3
"""Generate Wena's deterministic offline catalog from canonical WeKan JSON."""

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import struct
import zlib


MAGIC = b"WENA-I18N-1\n"
PLACEHOLDER = re.compile(r"__[A-Za-z0-9_]+__|%(?:[0-9]+\$)?[-+#0 ']*[0-9]*(?:\.[0-9]+)?[A-Za-z%]")


def placeholders(value):
    return Counter(PLACEHOLDER.findall(value))


def load_language(path, english_keys, english_values):
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise SystemExit(f"{path}: invalid UTF-8 JSON: {error}")
    if not isinstance(data, dict):
        raise SystemExit(f"{path}: catalog root must be an object")
    keys = list(data)
    if keys != english_keys:
        missing = [key for key in english_keys if key not in data]
        extra = [key for key in keys if key not in english_values]
        raise SystemExit(
            f"{path}: English key/order mismatch; missing={missing[:3]} extra={extra[:3]}"
        )
    for key, value in data.items():
        if not isinstance(value, str):
            raise SystemExit(f"{path}: {key}: translation must be a string")
        if placeholders(value) != placeholders(english_values[key]):
            raise SystemExit(f"{path}: {key}: placeholder inventory differs from English")
    return [[key, data[key]] for key in english_keys]


def generate(source_dir, source_revision):
    language_paths = sorted(source_dir.glob("*.i18n.json"), key=lambda path: path.name)
    english_path = source_dir / "en.i18n.json"
    if english_path not in language_paths:
        raise SystemExit(f"{english_path}: canonical English catalog is missing")
    english = json.loads(english_path.read_text(encoding="utf-8"))
    if not isinstance(english, dict):
        raise SystemExit(f"{english_path}: catalog root must be an object")
    english_keys = list(english)
    languages = []
    sources = []
    for path in language_paths:
        raw = path.read_bytes()
        sources.append([path.name, hashlib.sha256(raw).hexdigest()])
        languages.append({
            "tag": path.name[:-len(".i18n.json")],
            "entries": load_language(path, english_keys, english),
        })
    payload = json.dumps(
        {"format": 1, "source_revision": source_revision,
         "english_key_count": len(english_keys), "languages": languages,
         "sources": sources},
        ensure_ascii=False, separators=(",", ":"), sort_keys=False,
    ).encode("utf-8")
    compressed = zlib.compress(payload, level=9)
    return MAGIC + struct.pack(">II", len(payload), len(languages)) + compressed


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--source-revision", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    generated = generate(args.source, args.source_revision)
    if args.check:
        if not args.output.is_file() or args.output.read_bytes() != generated:
            raise SystemExit(f"stale or incomplete generated catalog: {args.output}")
        return
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(generated)
    print(f"wrote {args.output}: {len(generated)} bytes")


if __name__ == "__main__":
    main()
