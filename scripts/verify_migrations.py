#!/usr/bin/env python3
"""Fail when the pinned SQLite migration lock is stale or incomplete."""

import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def verify(root=ROOT):
    lock = json.loads((root / "config" / "migrations-lock.json").read_text(encoding="utf-8"))
    migration = (root / lock["migration_path"]).read_bytes()
    if lock != {
        "format": 1,
        "migration_path": "server/migrations/001_initial.sql",
        "migration_sha256": hashlib.sha256(migration).hexdigest(),
        "migration_size": len(migration),
        "schema_version": 1,
    }:
        raise SystemExit("SQLite migration lock is stale; regenerate and review it")
    if b"BEGIN" in migration or b"COMMIT" in migration or b"user_version" in migration:
        raise SystemExit("migration transaction/version must be owned by the runner")
    return lock, migration


if __name__ == "__main__":
    verify()
