#!/usr/bin/env python3
"""Verify ready targets embed the exact pinned migration before i18n."""

import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
SQL_MAGIC = b"WENA-SQL-END-v1!"
I18N_MAGIC = b"WENA-I18N-END-v1"


lock = json.loads((ROOT / "config" / "migrations-lock.json").read_text(encoding="utf-8"))
migration = (ROOT / lock["migration_path"]).read_bytes()
assert len(migration) == lock["migration_size"]
assert hashlib.sha256(migration).hexdigest() == lock["migration_sha256"]
subprocess.run(["python3", str(ROOT / "scripts" / "verify_migrations.py")], check=True)
verify_spec = importlib.util.spec_from_file_location("verify_migrations", ROOT / "scripts" / "verify_migrations.py")
verify_module = importlib.util.module_from_spec(verify_spec)
verify_spec.loader.exec_module(verify_module)
with tempfile.TemporaryDirectory() as temporary:
    stale = Path(temporary)
    (stale / "config").mkdir()
    (stale / "server" / "migrations").mkdir(parents=True)
    (stale / "config" / "migrations-lock.json").write_text(json.dumps(lock), encoding="utf-8")
    (stale / lock["migration_path"]).write_bytes(migration + b"-- stale\n")
    try:
        verify_module.verify(stale)
        raise AssertionError("stale migration unexpectedly passed")
    except SystemExit as error:
        assert "stale" in str(error)

ready = []
for line in (ROOT / "config" / "targets.tsv").read_text(encoding="utf-8").splitlines():
    if line and not line.startswith("#") and line.split("\t")[3] == "ready":
        ready.append(line.split("\t")[0])
for target in ready:
    script = (ROOT / ".github" / "release" / f"{target}.sh").read_text(encoding="utf-8")
    assert script.index("embed_migrations.py") < script.index("embed_i18n_catalog.py")

subprocess.run([str(ROOT / "build.sh"), "build", "host"], check=True)
spec = importlib.util.spec_from_file_location("wena_commands", ROOT / "scripts" / "wena.py")
wena = importlib.util.module_from_spec(spec)
spec.loader.exec_module(wena)
target = wena.host_target()
executable = ROOT / "dist" / target / "wena"
data = executable.read_bytes()
assert data.endswith(I18N_MAGIC)
i18n_size = struct.unpack(">Q", data[-24:-16])[0]
sql_footer_end = len(data) - 56 - i18n_size
sql_footer = data[sql_footer_end - 56:sql_footer_end]
assert sql_footer[40:] == SQL_MAGIC
sql_size = struct.unpack(">Q", sql_footer[32:40])[0]
embedded = data[sql_footer_end - 56 - sql_size:sql_footer_end - 56]
assert embedded == migration
