#!/usr/bin/env python3
"""Verify that one GitHub release contains the complete collected asset set."""

import argparse
import hashlib
import json
from pathlib import Path


def sha256(path: Path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def verify(local_dir: Path, response_path: Path):
    local = sorted(item for item in local_dir.iterdir() if item.is_file())
    non_files = sorted(item.name for item in local_dir.iterdir() if not item.is_file())
    if non_files:
        raise SystemExit("unexpected non-file release assets: " + ", ".join(non_files))
    if not local:
        raise SystemExit("no collected release assets to verify")

    response = json.loads(response_path.read_text(encoding="utf-8"))
    if not isinstance(response, list):
        raise SystemExit("GitHub release assets response is not a list")
    remote = {}
    for asset in response:
        name = asset.get("name") if isinstance(asset, dict) else None
        if not name:
            raise SystemExit("GitHub release asset lacks a name")
        if name in remote:
            raise SystemExit(f"duplicate remote release asset: {name}")
        remote[name] = asset

    failures = []
    for path in local:
        asset = remote.get(path.name)
        if asset is None:
            failures.append(f"missing remote asset: {path.name}")
            continue
        expected_size = path.stat().st_size
        if asset.get("size") != expected_size:
            failures.append(
                f"size mismatch for {path.name}: local {expected_size}, remote {asset.get('size')}"
            )
        remote_digest = asset.get("digest")
        if remote_digest:
            expected_digest = "sha256:" + sha256(path)
            if remote_digest != expected_digest:
                failures.append(
                    f"checksum mismatch for {path.name}: local {expected_digest}, remote {remote_digest}"
                )
    if failures:
        raise SystemExit("\n".join(failures))
    return len(local)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--local", type=Path, required=True)
    parser.add_argument("--response", type=Path, required=True)
    args = parser.parse_args()
    count = verify(args.local, args.response)
    print(f"verified {count} release assets by name and size; SHA-256 where supplied")


if __name__ == "__main__":
    main()
