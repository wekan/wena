#!/usr/bin/env python3
"""Shared local and CI command dispatcher for Wena."""

import platform
from pathlib import Path
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "config" / "targets.tsv"


def targets():
    result = []
    for number, line in enumerate(CATALOG.read_text(encoding="utf-8").splitlines(), 1):
        if not line or line.startswith("#"):
            continue
        fields = line.split("\t")
        if len(fields) != 5:
            raise SystemExit(f"targets.tsv:{number}: expected five fields")
        result.append(dict(zip(("target", "name", "runner", "status", "kind"), fields)))
    return result


def host_target(system=None, machine=None):
    system = (system or platform.system()).lower()
    machine = (machine or platform.machine()).lower()
    machines = {
        "x86_64": "amd64", "amd64": "amd64", "aarch64": "arm64",
        "arm64": "arm64", "armv7l": "armhf", "armv8l": "armhf",
    }
    cpu = machines.get(machine)
    systems = {"linux": "linux", "darwin": "macos", "windows": "windows"}
    operating_system = systems.get(system)
    if not operating_system or not cpu:
        raise SystemExit(f"unsupported current host: {system}/{machine}")
    return f"{operating_system}-{cpu}"


def shell_command(script):
    if sys.platform == "win32":
        shell = shutil.which("sh")
        if not shell:
            raise SystemExit("target build requires sh from Git for Windows or MSYS2")
        return [shell, str(script)]
    return [str(script)]


def build_one(target):
    record = next((item for item in targets() if item["target"] == target), None)
    if record is None:
        raise SystemExit(f"unknown target: {target}")
    if record["status"] != "ready":
        raise SystemExit(f"target is cataloged but not ready: {target}")
    script = ROOT / ".github" / "release" / f"{target}.sh"
    if not script.is_file():
        raise SystemExit(f"{target}: ready target is missing {script.relative_to(ROOT)}")
    if sys.platform != "win32" and not script.stat().st_mode & 0o111:
        raise SystemExit(f"{target}: {script.relative_to(ROOT)} exists but is not executable")
    verification = subprocess.call(
        [sys.executable, str(ROOT / "scripts" / "verify_i18n_catalog.py")], cwd=ROOT
    )
    if verification:
        return verification
    verification = subprocess.call(
        [sys.executable, str(ROOT / "scripts" / "verify_migrations.py")], cwd=ROOT
    )
    if verification:
        return verification
    print(f"Building {record['name']} ({target})", flush=True)
    return subprocess.call(shell_command(script), cwd=ROOT)


def build(selection):
    selected = host_target() if selection == "host" else selection
    records = [item for item in targets() if item["status"] == "ready"]
    names = [item["target"] for item in records] if selected == "all" else [selected]
    for name in names:
        result = build_one(name)
        if result:
            return result
    return 0


def list_targets():
    for item in targets():
        print("{target}\t{status}\t{name}".format(**item))
    return 0


def choose(title, choices):
    while True:
        print(f"\n{title}")
        for key, label in choices:
            print(f"  {key}) {label}")
        answer = input("Select: ").strip().lower()
        if any(answer == key for key, _label in choices):
            return answer
        print("Invalid selection.")


def build_menu():
    ready = [item for item in targets() if item["status"] == "ready"]
    choices = [("h", "Current host"), ("a", "All ready targets")]
    choices += [(str(index), f"{item['name']} ({item['target']})")
                for index, item in enumerate(ready, 1)]
    choices.append(("b", "Back"))
    answer = choose("Build", choices)
    if answer == "b":
        return
    selection = "host" if answer == "h" else "all" if answer == "a" else ready[int(answer) - 1]["target"]
    result = build(selection)
    if result:
        print(f"Build failed with exit code {result}.")


def tests_menu():
    answer = choose("Tests", [("1", "Strict-C89 models"),
                              ("2", "Locale normalization and fallback"),
                              ("3", "Language override and runtime switch"), ("b", "Back")])
    if answer == "1":
        result = run_test("models")
    elif answer == "2":
        result = run_test("locale")
    elif answer == "3":
        result = run_test("language")
    else:
        return
    if result:
        print(f"Tests failed with exit code {result}.")


def server_menu():
    answer = choose("Server", [("1", "Status"), ("b", "Back")])
    if answer == "1":
        print("Wena Server is not implemented yet; see ROADMAP.md.")


def tools_menu():
    answer = choose("Tools", [("1", "List target catalog"), ("b", "Back")])
    if answer == "1":
        list_targets()


def run_test(name):
    suites = {"models": ROOT / "tests" / "test_models.sh",
              "locale": ROOT / "tests" / "test_locale.sh",
              "language": ROOT / "tests" / "test_language.sh",
              "server-settings": ROOT / "tests" / "test_server_settings.sh",
              "html4-render": ROOT / "tests" / "test_legacy_html4_render.sh",
              "http-server": ROOT / "tests" / "test_http_server.sh",
              "security": ROOT / "tests" / "test_security.sh",
              "router": ROOT / "tests" / "test_router.sh",
              "platform-security": ROOT / "tests" / "test_platform_security.sh",
              "http-serving": ROOT / "tests" / "test_http_serving.sh",
              "capability": ROOT / "tests" / "test_capability.sh",
              "regions": ROOT / "tests" / "test_region_response.sh",
              "domain-operation": ROOT / "tests" / "test_domain_operation.sh",
              "persistence": ROOT / "tests" / "test_persistence.sh",
              "sqlite-schema": ROOT / "tests" / "test_sqlite_schema.sh"}
    suites["sqlite-storage"] = ROOT / "tests" / "test_sqlite_storage.sh"
    suites["progressive"] = ROOT / "tests" / "test_progressive_integration.sh"
    suites["migration-embed"] = ROOT / "tests" / "test_migration_embedding.py"
    script = suites.get(name)
    if script is None:
        raise SystemExit(f"unknown test suite: {name}")
    print(f"Running {name} tests", flush=True)
    return subprocess.call(shell_command(script), cwd=ROOT)


def menu():
    while True:
        answer = choose("Wena", [("1", "Build"), ("2", "Tests"),
                                  ("3", "Server"), ("4", "Tools"), ("q", "Quit")])
        if answer == "1":
            build_menu()
        elif answer == "2":
            tests_menu()
        elif answer == "3":
            server_menu()
        elif answer == "4":
            tools_menu()
        else:
            return 0


def usage():
    print("Usage: wena.py --list | build host|all|TARGET | tests --list | server status | tools targets | menu", file=sys.stderr)
    return 2


def main(argv):
    if argv == ["--list"]:
        return list_targets()
    if argv == ["menu"]:
        return menu()
    if len(argv) == 2 and argv[0] == "build":
        return build(argv[1])
    if argv == ["tests", "--list"]:
        print("models\tStrict-C89 model/unit and negative validation")
        print("locale\tOS locale normalization, fallback, and RTL direction")
        print("language\tPersistent override and immediate runtime switching")
        print("server-settings\tAdmin server address, ROOT_URL, and lifecycle state")
        print("html4-render\tROOT_URL-scoped escaped Legacy HTML4 baseline")
        print("http-server\tBounded parser and Admin-controlled IPv4 listener")
        print("security\tOpaque sessions and scoped single-use CSRF audit")
        print("router\tRead-only GET and protected mutation-intent gate")
        print("platform-security\tOS entropy and strict same-origin headers")
        print("http-serving\tTimed read-only HTML4 serving loop")
        print("capability\tProgressive drag/drop capability and baseline restore")
        print("regions\tBounded versioned visible-region response protocol")
        print("domain-operation\tVerified intent to allowlisted domain callback")
        print("persistence\tAtomic in-memory transaction and rollback contract")
        print("sqlite-schema\tVersioned SQLite schema and migration golden")
        print("sqlite-storage\tChecksummed atomic SQLite migration runner")
        print("progressive\tHTML4 fallback, DnD, POST, and multi-region integration")
        print("migration-embed\tPinned SQLite migration in every ready artifact")
        return 0
    if len(argv) == 2 and argv[0] == "tests":
        return run_test(argv[1])
    if argv == ["server", "status"]:
        print("Wena Server is not implemented yet; see ROADMAP.md.")
        return 3
    if argv == ["tools", "targets"]:
        return list_targets()
    return usage()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
