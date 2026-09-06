#!/usr/bin/env python3
"""Execute and inspect the pinned SQLite schema golden."""

from pathlib import Path
import hashlib
import sqlite3
import tempfile


ROOT = Path(__file__).resolve().parents[1]
MIGRATION = ROOT / "server" / "migrations" / "001_initial.sql"
EXPECTED_SHA256 = "e4760a2b70d6651ee84dce93642ccdd4ce8991b488dece5d231e66053f065da5"

assert hashlib.sha256(MIGRATION.read_bytes()).hexdigest() == EXPECTED_SHA256


with tempfile.TemporaryDirectory() as temporary:
    database = Path(temporary) / "wena.sqlite"
    connection = sqlite3.connect(database)
    connection.execute("PRAGMA foreign_keys = ON")
    connection.execute("PRAGMA journal_mode = WAL")
    connection.execute("PRAGMA synchronous = FULL")
    connection.executescript(MIGRATION.read_text(encoding="utf-8"))
    connection.execute("PRAGMA user_version = 1")
    assert connection.execute("PRAGMA user_version").fetchone()[0] == 1
    tables = {row[0] for row in connection.execute(
        "SELECT name FROM sqlite_schema WHERE type='table' AND name NOT LIKE 'sqlite_%'")}
    assert tables == {"schema_migrations", "actors", "sessions", "boards",
                      "swimlanes", "lists", "cards", "idempotency_keys"}
    assert connection.execute("PRAGMA foreign_keys").fetchone()[0] == 1
    assert connection.execute("PRAGMA journal_mode").fetchone()[0] == "wal"
    assert connection.execute("PRAGMA integrity_check").fetchone()[0] == "ok"
    assert list(connection.execute("PRAGMA foreign_key_check")) == []
    foreign_keys = {}
    for table in ("sessions", "swimlanes", "lists", "cards", "idempotency_keys"):
        foreign_keys[table] = {row[2] for row in connection.execute(
            f"PRAGMA foreign_key_list({table})")}
    assert foreign_keys == {
        "sessions": {"actors"}, "swimlanes": {"boards"}, "lists": {"boards"},
        "cards": {"boards", "swimlanes", "lists"}, "idempotency_keys": {"actors"},
    }
    connection.execute("INSERT INTO actors VALUES ('u1', 'User', 1)")
    connection.execute("INSERT INTO boards VALUES ('b1', 'Board', 1)")
    connection.execute("INSERT INTO swimlanes VALUES ('s1', 'b1', 'Lane', 0, 1)")
    connection.execute("INSERT INTO lists VALUES ('l1', 'b1', 'List', 0, 1)")
    connection.execute("INSERT INTO cards VALUES ('c1', 'b1', 's1', 'l1', 'Card', 0, 0, 1)")
    connection.execute("INSERT INTO idempotency_keys VALUES ('u1', '/b/b1/demo', 'create-card', 1, 'sha256:x', 1)")
    connection.commit()
    try:
        connection.execute("DELETE FROM boards WHERE id='b1'")
        connection.commit()
        raise AssertionError("parent delete unexpectedly succeeded")
    except sqlite3.IntegrityError:
        connection.rollback()
    try:
        connection.execute("INSERT INTO lists VALUES ('l2', 'b1', 'Other', 0, 1)")
        connection.commit()
        raise AssertionError("duplicate ordering position unexpectedly succeeded")
    except sqlite3.IntegrityError:
        connection.rollback()
    connection.execute("INSERT INTO boards VALUES ('b2', 'Other board', 1)")
    connection.execute("INSERT INTO lists VALUES ('l2', 'b2', 'Other list', 0, 1)")
    try:
        connection.execute("INSERT INTO cards VALUES ('cross', 'b1', 's1', 'l2', 'Bad', 1, 0, 1)")
        connection.commit()
        raise AssertionError("cross-board card unexpectedly succeeded")
    except sqlite3.IntegrityError:
        connection.rollback()
    connection.close()
