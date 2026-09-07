#!/usr/bin/env python3
"""Regression checks for shared interactive and noninteractive build dispatch."""

from contextlib import redirect_stdout
import importlib.util
import io
import sys
import threading
import time
from unittest.mock import patch
from pathlib import Path
import subprocess


ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("wena_commands", ROOT / "scripts" / "wena.py")
wena = importlib.util.module_from_spec(spec)
spec.loader.exec_module(wena)


def command(*arguments):
    return subprocess.run([str(ROOT / "build.sh"), *arguments], text=True, capture_output=True)


def test_runner():
    with patch.object(wena.subprocess, "call", return_value=0) as call:
        with redirect_stdout(io.StringIO()):
            assert wena.build("desktop") == 0
        assert call.call_args.args[0][-2] == str(ROOT / "scripts" / "build_desktop.sh")
        assert Path(call.call_args.args[0][-1]).parent == ROOT / "dist" / "desktop"
    with patch.object(wena.shutil, "which", return_value=None):
        assert "SDL2" in wena.test_prerequisite("desktop")
    with patch.object(wena.subprocess, "call", return_value=7) as call:
        assert wena.run_test("sanitizers") == 7
        assert call.call_args.args[0][-1] == str(ROOT / "tests" / "test_native_sanitizers.sh")
    names = [item[0] for item in wena.TEST_SUITES]
    assert len(names) == len(set(names))
    # Every suite which writes the shared host artifact must be serialized.
    for name, filename, _description in wena.TEST_SUITES:
        script = ROOT / "tests" / filename
        if script.is_file() and name != "build-entrypoints":
            source = script.read_text(encoding="utf-8")
            if ('build.sh" build host' in source or
                    '"build.sh"), "build", "host"' in source):
                assert name in wena.SERIAL_SUITES, name
    assert {"board-feature", "nuklear", "card-mutation", "build-entrypoints"} <= set(names)
    assert wena.test_command(Path("no-executable-bit.py")) == [sys.executable, "no-executable-bit.py"]
    with patch.object(wena.shutil, "which", return_value="/bin/sh"):
        assert wena.test_command(Path("suite.sh")) == ["/bin/sh", "suite.sh"]
    with patch.object(wena.subprocess, "run", side_effect=OSError("missing executable")):
        assert wena.execute_test(("models", "test_models.sh", ""))[1] == "FAIL"
    with patch.object(wena.subprocess, "run", side_effect=subprocess.TimeoutExpired("suite", 300)):
        assert wena.execute_test(("models", "test_models.sh", ""))[1] == "FAIL"
    records = [(name, "", "") for name in ("first", "second", "third", "migration-embed")]
    active = 0
    maximum = 0
    lock = threading.Lock()
    def fake(record):
        nonlocal active, maximum
        with lock:
            if record[0] == "migration-embed":
                assert active == 0, "shared-output build overlapped isolated tests"
            active += 1
            maximum = max(maximum, active)
        time.sleep(0.025 if record[0] == "first" else 0.01)
        with lock:
            active -= 1
        status = {"second": "FAIL", "third": "SKIP"}.get(record[0], "PASS")
        return record[0], status, "diagnostic"
    output = io.StringIO()
    with redirect_stdout(output):
        assert wena.run_all_tests(2, records, fake) == 1
    assert maximum == 2
    lines = output.getvalue()
    assert lines.index("PASS first") < lines.index("FAIL second") < lines.index("SKIP third")
    assert "2 pass, 1 fail, 1 skip" in lines
    with redirect_stdout(io.StringIO()):
        assert wena.run_all_tests(1, records, lambda record: (record[0], "PASS", "")) == 0
    for jobs in (0, 33):
        try:
            wena.run_all_tests(jobs, [], fake)
        except ValueError:
            pass
        else:
            raise AssertionError("unbounded worker count accepted")


def main():
    test_runner()
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
    for name, _filename, _description in wena.TEST_SUITES:
        assert name + "\t" in tests.stdout
    assert "all\t" in tests.stdout
    assert "models\tStrict-C89" in tests.stdout
    assert "locale\tOS locale" in tests.stdout
    assert "language\tPersistent override" in tests.stdout
    assert "server-settings\tAdmin server" in tests.stdout
    assert "html4-render\tROOT_URL" in tests.stdout
    assert "http-server\tBounded parser" in tests.stdout
    assert "security\tOpaque sessions" in tests.stdout
    assert "router\tRead-only GET" in tests.stdout
    assert "platform-security\tOS entropy" in tests.stdout
    assert "http-serving\tTimed read-only" in tests.stdout
    assert "capability\tProgressive drag/drop" in tests.stdout
    assert "regions\tBounded versioned" in tests.stdout
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
