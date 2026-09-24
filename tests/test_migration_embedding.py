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


verify_spec = importlib.util.spec_from_file_location("verify_migrations", ROOT / "scripts" / "verify_migrations.py")
verify_module = importlib.util.module_from_spec(verify_spec)
verify_spec.loader.exec_module(verify_module)
lock, migration = verify_module.verify()
assert lock["schema_version"] == 7
assert lock["migrations"][0]["sha256"] == verify_module.V1_SHA256
assert migration.startswith((ROOT / lock["migrations"][0]["path"]).read_bytes())
assert len(migration) == lock["migrations"][-1]["bundle_size"]
assert hashlib.sha256(migration).hexdigest() == lock["migrations"][-1]["bundle_sha256"]
subprocess.run(["python3", str(ROOT / "scripts" / "verify_migrations.py"), "--check"], check=True)
with tempfile.TemporaryDirectory() as temporary:
    stale = Path(temporary)
    (stale / "config").mkdir()
    (stale / "server" / "migrations").mkdir(parents=True)
    def reset():
        (stale / "config" / "migrations-lock.json").write_text(json.dumps(lock), encoding="utf-8")
        for entry in lock["migrations"]:
            (stale / entry["path"]).write_bytes((ROOT / entry["path"]).read_bytes())
        (stale / verify_module.HEADER).write_bytes((ROOT / verify_module.HEADER).read_bytes())
    for case in range(14):
        reset()
        altered = json.loads(json.dumps(lock))
        if case == 0:
            path = stale / lock["migrations"][0]["path"]
            path.write_bytes(path.read_bytes() + b"-- immutable\n")
        elif case == 1:
            path = stale / lock["migrations"][1]["path"]
            path.write_bytes(path.read_bytes() + b"-- stale\n")
        elif case == 2:
            path = stale / verify_module.HEADER
            path.write_bytes(path.read_bytes() + b"/* stale */\n")
        elif case == 9:
            path = stale / lock["migrations"][2]["path"]
            path.write_bytes(path.read_bytes() + b"-- stale v3\n")
        elif case == 10:
            path = stale / lock["migrations"][3]["path"]
            path.write_bytes(path.read_bytes() + b"-- stale v4\n")
        elif case == 11:
            path = stale / lock["migrations"][4]["path"]
            path.write_bytes(path.read_bytes() + b"-- stale v5\n")
        elif case == 12:
            path = stale / lock["migrations"][5]["path"]
            path.write_bytes(path.read_bytes() + b"-- stale v6\n")
        elif case == 13:
            path = stale / lock["migrations"][6]["path"]
            path.write_bytes(path.read_bytes() + b"-- stale v7\n")
        else:
            if case == 3: altered["migrations"].reverse()
            elif case == 4: altered["migrations"].pop()
            elif case == 5: altered["migrations"][1]["version"] = 3
            elif case == 6: altered["migrations"][1]["bundle_sha256"] = "0" * 64
            elif case == 7: altered["schema_version"] = lock["schema_version"] + 1
            else: altered["unexpected"] = True
            (stale / "config" / "migrations-lock.json").write_text(json.dumps(altered), encoding="utf-8")
        try:
            verify_module.verify(stale)
            raise AssertionError(f"altered migration fixture {case} unexpectedly passed")
        except SystemExit:
            pass

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
