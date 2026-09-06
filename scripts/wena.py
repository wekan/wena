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
    answer = choose("Tests", [("1", "Strict-C89 models"), ("b", "Back")])
    if answer == "1":
        result = run_test("models")
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
    suites = {"models": ROOT / "tests" / "test_models.sh"}
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
