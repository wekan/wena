#!/usr/bin/env python3
"""FerretDB reads what Wena writes, and Wena changes what FerretDB wrote.

With the FerretDB binary WeKan bundles and the MongoDB Node.js driver - both
optional here, the test says when it skipped - a wekan.sqlite written by Wena
is served by FerretDB and read back through the MongoDB protocol with every
type intact; then a document inserted through FerretDB, with fields Wena knows
nothing of, is updated and has a field removed by Wena, and FerretDB reads
it with those other fields as they were.

  FERRETDB_BIN       the binary (default: ../FerretDB/bin/ferretdb)
  WENA_NODE_MODULES  a node_modules with "mongodb" (default: WeKan's)
"""
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def node(script, port, modules):
    env = dict(os.environ, NODE_PATH=str(modules))
    run = subprocess.run(["node", "-e", script.replace("PORT", str(port))], capture_output=True, text=True,
                         env=env, timeout=60)
    assert run.returncode == 0, run.stdout + run.stderr
    return json.loads(run.stdout)


def start(ferretdb, directory, port):
    server = subprocess.Popen([str(ferretdb), "--handler=sqlite", f"--sqlite-url=file:{directory}/",
                               f"--listen-addr=127.0.0.1:{port}", "--telemetry=disable", "--log-level=error"],
                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    for _ in range(100):
        try:
            socket.create_connection(("127.0.0.1", port), timeout=0.2).close()
            break
        except OSError:
            time.sleep(0.1)
    return server


def stop(server):
    server.terminate()
    server.wait(timeout=30)


def main():
    ferretdb = Path(os.environ.get("FERRETDB_BIN") or ROOT.parent / "FerretDB" / "bin" / "ferretdb")
    modules = Path(os.environ.get("WENA_NODE_MODULES") or ROOT.parents[1] / "node_modules")
    if not ferretdb.is_file() or shutil.which("node") is None or not (modules / "mongodb").is_dir():
        print("ferretdb-roundtrip: skipped (needs FerretDB, node and the mongodb driver)")
        return
    with tempfile.TemporaryDirectory() as temp:
        temp = Path(temp)
        tool = temp / "tool"
        subprocess.run(["cc", "-std=c89", "-pedantic-errors", "-Wall", "-Wextra", "-Werror",
                        str(ROOT / "tests" / "ferretdb_compat_tool.c"), str(ROOT / "server" / "ferretdb_sqlite.c"),
                        "-lsqlite3", "-o", str(tool)], check=True)
        db = temp / "db"
        db.mkdir()
        subprocess.run([str(tool), "write", str(db)], check=True)
        port = free_port()
        server = subprocess.Popen([str(ferretdb), "--handler=sqlite", f"--sqlite-url=file:{db}/",
                                   f"--listen-addr=127.0.0.1:{port}", "--telemetry=disable", "--log-level=error"],
                                  stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            for _ in range(100):
                try:
                    socket.create_connection(("127.0.0.1", port), timeout=0.2).close()
                    break
                except OSError:
                    time.sleep(0.1)
            read = """
const { MongoClient } = require('mongodb');
(async () => { const c = new MongoClient('mongodb://127.0.0.1:PORT/wekan?directConnection=true');
  await c.connect(); const b = c.db('wekan').collection('boards');
  const d = await b.findOne({_id: 'wena-b1'});
  const out = {doc: d, created: d.createdAt instanceof Date ? d.createdAt.getTime() : null,
    keys: Object.keys(d), byTitle: await b.countDocuments({title: 'Wena board'})};
  await b.insertOne({_id: 'kanban1', title: 'From WeKan', description: 'gone soon', archived: true,
    modifiedAt: new Date(1700000001000), labels: [{_id: 'l1', name: 'Urgent', color: 'red'}], sort: 2});
  console.log(JSON.stringify(out)); await c.close(); })().catch(e => { console.error(e); process.exit(1); });
"""
            got = node(read, port, modules)
            # Every field, in order, with its type: the date is a Date.
            assert got["keys"] == ["_id", "title", "archived", "createdAt", "sort", "members", "stars"], got
            assert got["created"] == 1700000000000 and got["doc"]["sort"] == 1.5 and got["doc"]["stars"] == 3
            assert got["doc"]["members"] == [{"userId": "u1", "isAdmin": True}] and got["byTitle"] == 1
        finally:
            server.terminate()
            server.wait(timeout=30)
        # Wena changes the document FerretDB wrote.
        subprocess.run([str(tool), "update", str(db)], check=True)
        server = subprocess.Popen([str(ferretdb), "--handler=sqlite", f"--sqlite-url=file:{db}/",
                                   f"--listen-addr=127.0.0.1:{port}", "--telemetry=disable", "--log-level=error"],
                                  stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            for _ in range(100):
                try:
                    socket.create_connection(("127.0.0.1", port), timeout=0.2).close()
                    break
                except OSError:
                    time.sleep(0.1)
            again = """
const { MongoClient } = require('mongodb');
(async () => { const c = new MongoClient('mongodb://127.0.0.1:PORT/wekan?directConnection=true');
  await c.connect(); const d = await c.db('wekan').collection('boards').findOne({_id: 'kanban1'});
  console.log(JSON.stringify({doc: d, keys: Object.keys(d),
    modified: d.modifiedAt instanceof Date ? d.modifiedAt.getTime() : null}));
  await c.close(); })().catch(e => { console.error(e); process.exit(1); });
"""
            got = node(again, port, modules)
            assert got["keys"] == ["_id", "title", "archived", "modifiedAt", "labels", "sort", "wenaNew"], got
            assert got["doc"]["title"] == "Renamed by Wena" and got["doc"]["archived"] is True
            assert got["modified"] == 1700000001000 and got["doc"]["labels"][0]["name"] == "Urgent"
            assert "description" not in got["doc"] and got["doc"]["wenaNew"] is True
        finally:
            server.terminate()
            server.wait(timeout=30)
        # WeKan's documents written through FerretDB, Wena's whole path over
        # them (import, change, export), and FerretDB serving the result.
        sync_tool = temp / "sync"
        subprocess.run(["cc", "-std=c89", "-pedantic-errors", "-Wall", "-Wextra", "-Werror",
                        str(ROOT / "tests" / "wekan_sync_tool.c"), str(ROOT / "server" / "wekan_sync.c"),
                        str(ROOT / "server" / "ferretdb_sqlite.c"), str(ROOT / "server" / "sqlite_storage.c"),
                        str(ROOT / "server" / "sha256.c"), "-lsqlite3", "-o", str(sync_tool)], check=True)
        wekan = temp / "wekan"
        wekan.mkdir()
        server = start(ferretdb, wekan, port)
        try:
            node("""
const { MongoClient } = require('mongodb');
(async () => { const c = new MongoClient('mongodb://127.0.0.1:PORT/wekan?directConnection=true');
  await c.connect(); const db = c.db('wekan'); const now = new Date(1700000000000);
  await db.collection('users').insertOne({_id: 'u1', username: 'ada', profile: {fullname: 'Ada'}, isAdmin: true, createdAt: now});
  await db.collection('boards').insertOne({_id: 'b1', title: 'WeKan board', slug: 'wekan-board', archived: false, type: 'board',
    permission: 'private', color: 'belize', members: [{userId: 'u1', isAdmin: true, isActive: true}], labels: [], createdAt: now});
  await db.collection('swimlanes').insertOne({_id: 's1', title: 'Default', boardId: 'b1', sort: 0, archived: false, createdAt: now});
  await db.collection('lists').insertOne({_id: 'l1', title: 'To Do', boardId: 'b1', sort: 0, archived: false, createdAt: now});
  await db.collection('cards').insertOne({_id: 'c1', title: 'From WeKan', boardId: 'b1', listId: 'l1', swimlaneId: 's1',
    sort: 0, archived: false, type: 'cardType-card', userId: 'u1', dueAt: now, createdAt: now, labelIds: []});
  await db.collection('checklists').insertOne({_id: 'k1', cardId: 'c1', boardId: 'b1', title: 'Steps', sort: 0, createdAt: now});
  await db.collection('checklistItems').insertOne({_id: 'i1', checklistId: 'k1', cardId: 'c1', boardId: 'b1', title: 'One',
    sort: 0, isFinished: false, createdAt: now});
  console.log('{}'); await c.close(); })().catch(e => { console.error(e); process.exit(1); });
""", port, modules)
        finally:
            stop(server)
        subprocess.run([str(sync_tool), str(wekan)], check=True)
        server = start(ferretdb, wekan, port)
        try:
            got = node("""
const { MongoClient } = require('mongodb');
(async () => { const c = new MongoClient('mongodb://127.0.0.1:PORT/wekan?directConnection=true');
  await c.connect(); const db = c.db('wekan');
  const c1 = await db.collection('cards').findOne({_id: 'c1'});
  const made = await db.collection('cards').findOne({_id: 'wena-new'});
  const item = await db.collection('checklistItems').findOne({_id: 'i1'});
  const onBoard = await db.collection('cards').find({boardId: 'b1', archived: false}).sort({sort: 1}).toArray();
  console.log(JSON.stringify({c1, made, item, titles: onBoard.map(x => x.title),
    due: c1.dueAt instanceof Date, created: made.createdAt instanceof Date}));
  await c.close(); })().catch(e => { console.error(e); process.exit(1); });
""", port, modules)
        finally:
            stop(server)
        assert got["c1"]["title"] == "Renamed in Wena" and got["due"], got
        assert got["made"]["title"] == "Made in Wena" and got["made"]["type"] == "cardType-card", got
        assert got["made"]["userId"] == "u1" and got["made"]["listId"] == "l1" and got["created"], got
        assert got["item"]["isFinished"] is True and got["titles"] == ["Renamed in Wena", "Made in Wena"], got
    print("ferretdb-roundtrip: FerretDB reads Wena's documents and Wena keeps FerretDB's fields")


if __name__ == "__main__":
    main()
