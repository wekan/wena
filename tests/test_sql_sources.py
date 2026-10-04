#!/usr/bin/env python3
"""No SQL executed by Wena or its tests comes from outside the program.

GitHub CodeQL reported 18 tests (cpp/sql-injection, "Uncontrolled data in SQL
query") that read a schema file named on their command line and passed its
text to sqlite3_exec. The same shape was in 60 tests. Their schema and fixture
SQL is now compiled in by scripts/embed_test_files.py and read through
tests/support/test_files.h; a path argument only selects one of those files.

This guard keeps it that way:
- a test that runs SQL reads no file at run time, except the reviewed ones
  below, which read back binary database files and never execute them;
- a test script that hands a .sql file to its test also embeds it;
- the application builds SQL text only in the reviewed places below, from
  fixed table and column names (and SQLite's %w identifier quoting).
"""

from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
# Calls that execute SQL or a migration the caller hands them.
SQL_CALL = re.compile(r"\bsqlite3_(exec|prepare_v2|prepare_v3|prepare)\s*\(|\bwena_sqlite_open\s*\(|"
                      r"\bwena_server_runtime_start\s*\(|\bwena_sqlite_restore\s*\(|\bsql\s*\(")
FILE_READ = re.compile(r"\b(fread|fgets|getline|fscanf)\s*\(")
# Tests that run SQL and also read a file - never as SQL. Reviewed.
READS_NO_SQL = {
    "ferretdb_scan_test.c": "compares the database file's bytes before and after a read-only scan",
    "sqlite_workspace_test.c": "reads back a 16-byte sentinel file a workspace create must not replace",
    "sqlite_backup_test.c": "compares backup and database file bytes",
    "sqlite_restore_test.c": "hashes a backup database file for its sidecar",
    "sqlite_schema_v2_test.c": "hashes database files",
    "sqlite_schema_v3_test.c": "hashes backup and database files",
    "sqlite_schema_v4_test.c": "hashes backup files",
    "sqlite_schema_v5_test.c": "hashes backup and database files",
    "sqlite_schema_v6_test.c": "hashes backup and database files",
}
# Application files that format SQL text, and why each is safe. A new one
# fails here until it is reviewed and listed.
APP_SQL_TEXT = {
    "server/mutations/hierarchy_colors.c": "table/column names chosen by a list-or-swimlane flag",
    "server/sqlite_board.c": "table/column names chosen by an integer kind",
    "server/sqlite_persistence.c": "a constant clause chosen by a flag",
    "server/sqlite_storage.c": "an integer migration version",
    "server/ferretdb_scan.c": "a table name quoted with sqlite3_mprintf %w",
    "server/ferretdb_sqlite.c": "schema/table names refused when they hold a quote, and field names "
                                "limited to [A-Za-z0-9_$-] before they reach a JSON path",
    "server/wekan_sync.c": "collection table names FerretDB derives (FNV-1a), into fixed SQL of this file",
    "server/wekan_views.c": "collection table names from FerretDB's metadata (quoted) and fixed clauses of this "
                            "file; every value is bound",
}


def strip_comments(text):
    return re.sub(r"/\*.*?\*/", "", re.sub(r"//[^\n]*", "", text), flags=re.S)


def test_sources_read_no_sql():
    problems = []
    for path in sorted((ROOT / "tests").glob("*.c")):
        text = strip_comments(path.read_text(encoding="utf-8", errors="replace"))
        if SQL_CALL.search(text) and FILE_READ.search(text) and path.name not in READS_NO_SQL:
            problems.append(f"tests/{path.name} runs SQL and reads a file at run time; "
                            "compile the SQL in (tests/support/test_files.h)")
    for name in READS_NO_SQL:
        assert (ROOT / "tests" / name).is_file(), f"stale allow-list entry {name}"
    assert not problems, "\n".join(problems)


def test_scripts_embed_the_sql_they_pass():
    problems = []
    for script in sorted((ROOT / "tests").glob("test_*.sh")):
        text = script.read_text(encoding="utf-8")
        runs = [line for line in text.splitlines()
                if re.match(r'\s*"\$\{?test_dir\}?/[\w.-]+"', line) and re.search(r"\.sql(?![a-z])", line)]
        if runs and "embed_test_files.py" not in text:
            problems.append(f"tests/{script.name} passes SQL files to its test without embedding them")
        if "embed_test_files.py" in text and "tests/support/test_files.c" not in text:
            problems.append(f"tests/{script.name} embeds files but does not compile tests/support/test_files.c")
    assert not problems, "\n".join(problems)


def test_application_builds_sql_text_only_where_reviewed():
    found = set()
    for folder in ("server", "client", "imports", "models"):
        for path in (ROOT / folder).rglob("*.c"):
            text = strip_comments(path.read_text(encoding="utf-8", errors="replace"))
            if re.search(r"sqlite3_v?mprintf|sqlite3_snprintf", text) or \
                    re.search(r"sprintf\s*\([^;]*\b(SELECT|INSERT|UPDATE|DELETE|PRAGMA|CREATE|DROP|ALTER)\b", text) or \
                    re.search(r"sprintf\s*\(\s*\w+\s*,\s*sql\b", text):
                found.add(path.relative_to(ROOT).as_posix())
    assert found == set(APP_SQL_TEXT), (
        "SQL text is built in " + ", ".join(sorted(found - set(APP_SQL_TEXT))) +
        " - bind values with sqlite3_bind_*, or review and list it here" if found - set(APP_SQL_TEXT)
        else "stale entries: " + ", ".join(sorted(set(APP_SQL_TEXT) - found)))
    scan = (ROOT / "server" / "ferretdb_scan.c").read_text(encoding="utf-8")
    assert 'FROM \\"%w\\"' in scan, "ferretdb_scan.c must quote the table name with %w"


def test_embedding_generator():
    spec = __import__("importlib.util").util.spec_from_file_location("embed", ROOT / "scripts" / "embed_test_files.py")
    embed = __import__("importlib.util").util.module_from_spec(spec)
    spec.loader.exec_module(embed)
    with tempfile.TemporaryDirectory() as temp:
        one, two = Path(temp) / "one.sql", Path(temp) / "sub"
        two.mkdir()
        one.write_bytes(b"CREATE TABLE t(x);")
        (two / "one.sql").write_bytes(b"DROP TABLE t;")
        header = embed.header([str(one)])
        assert '{"one.sql",wena_test_file_0,18u}' in header and "WENA_TEST_FILE_COUNT 1u" in header
        assert header.count(",0};") == 1, "NUL-terminated"
        try:
            embed.header([str(one), str(two / "one.sql")])
        except ValueError:
            pass
        else:
            raise AssertionError("two files with one name were embedded")
        # The lookup, compiled and run: by last path component, exact bytes;
        # an unknown name aborts the test rather than running nothing.
        (Path(temp) / "wena_test_files.h").write_text(embed.header([str(one)]), encoding="ascii")
        (Path(temp) / "probe.c").write_text(
            '#include "support/test_files.h"\n#include <string.h>\n#include <stdlib.h>\n'
            'int main(int c,char**v){size_t n;long m;const char*s;char*d;(void)c;'
            's=wena_test_file("/any/where/one.sql",&n);if(n!=18||strcmp(s,"CREATE TABLE t(x);"))return 1;'
            'd=wena_test_file_copy("one.sql",&m);if(m!=18||strcmp(d,s))return 2;free(d);'
            'if(v[1])wena_test_file(v[1],NULL);return 0;}\n', encoding="ascii")
        binary = Path(temp) / "probe"
        subprocess.run(["cc", "-std=c89", "-pedantic-errors", "-Wall", "-Wextra", "-Werror",
                        "-I", temp, "-I", str(ROOT / "tests"), str(Path(temp) / "probe.c"),
                        str(ROOT / "tests" / "support" / "test_files.c"), "-o", str(binary)], check=True)
        assert subprocess.run([str(binary)]).returncode == 0
        missing = subprocess.run([str(binary), "two.sql"], capture_output=True, text=True)
        assert missing.returncode != 0 and "was not embedded" in missing.stderr


def main():
    test_sources_read_no_sql()
    test_scripts_embed_the_sql_they_pass()
    test_application_builds_sql_text_only_where_reviewed()
    test_embedding_generator()
    print("sql sources: no test or application SQL comes from outside the program")
    return 0


if __name__ == "__main__":
    sys.exit(main())
