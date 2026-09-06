#!/usr/bin/env python3
"""Regression checks for shared interactive and noninteractive build dispatch."""

import importlib.util
from pathlib import Path
import subprocess


ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("wena_commands", ROOT / "scripts" / "wena.py")
wena = importlib.util.module_from_spec(spec)
spec.loader.exec_module(wena)


def command(*arguments):
    return subprocess.run([str(ROOT / "build.sh"), *arguments], text=True, capture_output=True)


def main():
    assert wena.host_target("Linux", "x86_64") == "linux-amd64"
    assert wena.host_target("Darwin", "arm64") == "macos-arm64"
    assert wena.host_target("Windows", "AMD64") == "windows-amd64"

    listed = command("--list")
    assert listed.returncode == 0, listed.stderr
    ready = [item for item in wena.targets() if item["status"] == "ready"]
    for item in ready:
        assert f"{item['target']}\tready\t{item['name']}" in listed.stdout

    planned = command("build", "linux-i686")
    assert planned.returncode != 0
    assert "cataloged but not ready" in planned.stderr
    unknown = command("build", "not-a-target")
    assert unknown.returncode != 0
    assert "unknown target" in unknown.stderr
    usage = command("unexpected")
    assert usage.returncode == 2
    assert "Usage:" in usage.stderr

    no_terminal = command()
    assert no_terminal.returncode == 2
    assert "Interactive menu requires a terminal" in no_terminal.stderr

    tests = command("tests", "--list")
    assert tests.returncode == 0
    assert "models\tStrict-C89" in tests.stdout
    assert "locale\tOS locale" in tests.stdout
    assert "language\tPersistent override" in tests.stdout
    assert "server-settings\tAdmin server" in tests.stdout
    assert "html4-render\tROOT_URL" in tests.stdout
    assert "http-server\tBounded parser" in tests.stdout
    assert "security\tOpaque sessions" in tests.stdout
    assert "router\tRead-only GET" in tests.stdout
    assert "platform-security\tOS entropy" in tests.stdout
    model_tests = command("tests", "models")
    assert model_tests.returncode == 0, model_tests.stderr
    locale_tests = command("tests", "locale")
    assert locale_tests.returncode == 0, locale_tests.stderr
    language_tests = command("tests", "language")
    assert language_tests.returncode == 0, language_tests.stderr
    bad_tests = command("tests", "unknown")
    assert bad_tests.returncode != 0
    assert "unknown test suite" in bad_tests.stderr
    server = command("server", "status")
    assert server.returncode == 3
    tools = command("tools", "targets")
    assert tools.returncode == 0
    assert tools.stdout == listed.stdout

    shell_entry = (ROOT / "build.sh").read_text(encoding="utf-8")
    batch_entry = (ROOT / "build.bat").read_text(encoding="utf-8")
    dispatcher = (ROOT / "scripts" / "wena.py").read_text(encoding="utf-8")
    assert "scripts/wena.py" in shell_entry
    assert "scripts\\wena.py" in batch_entry
    assert "\npause" not in shell_entry.lower()
    assert "\npause" not in batch_entry.lower()
    assert "ready target is missing" in dispatcher
    assert "exists but is not executable" in dispatcher
    assert "verify_i18n_catalog.py" in dispatcher
    for category in ("Build", "Tests", "Server", "Tools"):
        assert category in dispatcher


if __name__ == "__main__":
    main()
