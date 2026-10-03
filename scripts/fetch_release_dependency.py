#!/usr/bin/env python3
"""Download one pinned release dependency into a cache and check it.

    fetch_release_dependency.py NAME CACHE_DIR    -> prints the file's path

NAME is a key of config/release-dependencies.json. A cached file is used only
when its SHA-256 matches the pin; a download that does not match is deleted
and refused, so nothing unchecked is ever built.
"""

import hashlib
import json
from pathlib import Path
import sys
import urllib.request

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    hasher = hashlib.sha256()
    with open(path, "rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            hasher.update(block)
    return hasher.hexdigest()


def fetch(name, cache, pins=None, download=urllib.request.urlretrieve):
    pins = pins or json.loads((ROOT / "config" / "release-dependencies.json").read_text())
    pin = pins.get(name)
    if not isinstance(pin, dict) or name == "format":
        raise ValueError(f"no pinned release dependency named {name}")
    cache = Path(cache)
    cache.mkdir(parents=True, exist_ok=True)
    path = cache / pin["url"].rsplit("/", 1)[1]
    if path.is_file() and digest(path) == pin["sha256"]:
        return path
    partial = path.with_name(path.name + ".part")
    download(pin["url"], partial)
    actual = digest(partial)
    if actual != pin["sha256"]:
        partial.unlink()
        raise ValueError(f"{name}: downloaded SHA-256 {actual} does not match the pin {pin['sha256']}")
    partial.replace(path)
    return path


def main(argv):
    if len(argv) != 2:
        print("Usage: fetch_release_dependency.py NAME CACHE_DIR", file=sys.stderr)
        return 2
    try:
        print(fetch(argv[0], argv[1]))
    except (OSError, ValueError, KeyError) as error:
        print(f"fetch_release_dependency: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
