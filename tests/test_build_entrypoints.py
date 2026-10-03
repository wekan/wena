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
import re
import subprocess
import tempfile


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
        assert call.call_args.args[0][1] == "-c"
        assert "for suite in" in call.call_args.args[0][2]
    with patch.object(wena.subprocess, "call", return_value=0) as call:
        assert wena.build("desktop-package") == 0
        assert call.call_args.args[0] == [sys.executable, str(ROOT / "scripts/package_desktop.py")]
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


def test_run():
    # 2) Run opens the native Nuklear desktop that 1) Build > d) wrote.
    assert wena.desktop_binary("/r", False) == Path("/r/dist/desktop/wena-desktop")
    assert wena.desktop_binary("/r", True) == Path("/r/dist/desktop/wena-desktop.exe")
    # Its board lives in the user's data folder, or where WENA_DATABASE says.
    assert wena.workspace_path({}, "Darwin", "/home/u") == Path(
        "/home/u/Library/Application Support/Wena/wena.sqlite")
    assert wena.workspace_path({}, "Linux", "/home/u") == Path("/home/u/.local/share/wena/wena.sqlite")
    assert wena.workspace_path({"XDG_DATA_HOME": "/data"}, "Linux", "/home/u") == Path("/data/wena/wena.sqlite")
    assert wena.workspace_path({"WENA_DATABASE": "/x/board.sqlite"}, "Linux", "/home/u") == Path("/x/board.sqlite")
    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp)
        database = root / "data" / "wena.sqlite"
        # Created on the first run, reopened afterwards.
        assert wena.desktop_arguments(database) == ["--database", str(database), "--actor", "local-user",
                                                    "--board", "my-board", "--create", "--title", "My board"]
        # Negative: nothing built yet says how to build it, and runs nothing.
        errors = io.StringIO()
        with patch.object(wena.subprocess, "call") as call, patch.object(sys, "stderr", errors):
            assert wena.run((), root, database) == 1
        call.assert_not_called()
        assert "not built yet" in errors.getvalue() and "d) Local SDL2/SQLite desktop app" in errors.getvalue()
        binary = wena.desktop_binary(root)
        binary.parent.mkdir(parents=True)
        binary.write_text("#!/bin/sh\necho \"ran $*\"\nexit 7\n", encoding="utf-8")
        if sys.platform != "win32":
            # Negative: a file that cannot be executed is refused.
            binary.chmod(0o644)
            with patch.object(wena.subprocess, "call") as call, patch.object(sys, "stderr", io.StringIO()):
                assert wena.run((), root, database) == 1
            call.assert_not_called()
            binary.chmod(0o755)
            # Without arguments it opens the default board, creating its folder;
            # its exit code is returned and its output kept in this run's log.
            now = wena.datetime(2026, 10, 3, 15, 4, 5)
            logs = wena.log_directory(root, now)
            assert logs == root / ".tools" / "log" / "wena" / "2026-10-03_15-04-05"
            assert wena.log_directory(Path("/r/.tools/wena"), now) == Path("/r/.tools/log/wena/2026-10-03_15-04-05")
            binary.write_text("#!/bin/sh\necho \"ran $*\"\necho \"log $WENA_LOG_DIR\"\nexit 7\n", encoding="utf-8")
            with redirect_stdout(io.StringIO()) as printed:
                assert wena.run((), root, database, now) == 7
            assert database.parent.is_dir()
            record = (logs / "run.log").read_text(encoding="utf-8")
            assert record.startswith("command: " + " ".join([str(binary), *wena.desktop_arguments(database)]))
            assert "ran --database " + str(database) in record and f"log {logs}" in record
            assert record.rstrip().endswith("ended: exit code 7")
            assert f"Debug log: {logs}" in printed.getvalue()
            database.write_bytes(b"")
            assert "--create" not in wena.desktop_arguments(database)
            # A crash is recorded as the signal that ended it.
            binary.write_text("#!/bin/sh\nkill -SEGV $$\n", encoding="utf-8")
            with redirect_stdout(io.StringIO()):
                assert wena.run(("--smoke",), root, database, wena.datetime(2026, 10, 3, 15, 4, 6)) < 0
            assert "ended: killed by signal 11" in (
                wena.log_directory(root, wena.datetime(2026, 10, 3, 15, 4, 6)) / "run.log").read_text(encoding="utf-8")
            # Given arguments are passed as they are, to the real file.
            binary.write_text("#!/bin/sh\necho \"ran $*\"\nexit 7\n", encoding="utf-8")
            out = subprocess.run([sys.executable, "-c",
                                  "import importlib.util,sys;"
                                  f"s=importlib.util.spec_from_file_location('w',{str(ROOT / 'scripts' / 'wena.py')!r});"
                                  "w=importlib.util.module_from_spec(s);s.loader.exec_module(w);"
                                  f"sys.exit(w.run(['--help'],{temp!r}))"],
                                 text=True, capture_output=True)
            assert out.returncode == 7, out.stderr
            assert "ran --help" in out.stdout
    # The menu: 1) Build, 2) Run, 3) Release, and the rest one number lower than before.
    answers = iter(["2", "3", "4", "5", "6", "q"])
    printed = io.StringIO()
    with patch("builtins.input", lambda _prompt: next(answers)), \
            patch.object(wena, "run", return_value=0) as ran, \
            patch.object(wena, "release_menu") as released, \
            patch.object(wena, "tests_menu") as tests, \
            patch.object(wena, "server_menu") as server, \
            patch.object(wena, "tools_menu") as tools, redirect_stdout(printed):
        assert wena.menu() == 0
    ran.assert_called_once_with()
    assert (released.call_count, tests.call_count, server.call_count, tools.call_count) == (1, 1, 1, 1)
    text = printed.getvalue()
    assert "1) Build\n  2) Run\n  3) Release\n  4) Tests\n  5) Server\n  6) Tools\n  q) Quit" in text


class Completed:
    def __init__(self, returncode):
        self.returncode = returncode


def test_release():
    # 3) Release starts the GitHub workflows with gh for the pushed branch.
    # Nothing here reaches GitHub: gh and git are replaced.
    assert wena.github_repository("git@github.com:wekan/wena") == "wekan/wena"
    assert wena.github_repository("https://github.com/wekan/wena.git") == "wekan/wena"
    assert wena.github_repository("/srv/git/wena.git") is None
    assert wena.github_repository("https://github.com/wekan") is None
    calls, sleeps = [], []

    def fake(results, git=None):
        git = {"remote": "git@github.com:wekan/wena", "rev-parse": "main", "rev-list": "0", **(git or {})}
        results = list(results)

        def run(command, **_kwargs):
            calls.append(command)
            if command[:3] == ["gh", "auth", "status"]:
                if results and results[0] == "auth-fail":
                    results.pop(0)
                    return Completed(1)
                return Completed(0)
            if command[:3] == ["gh", "workflow", "run"]:
                return Completed(results.pop(0) if results else 0)
            return Completed(0)
        return run, (lambda *args, root=None: git[args[0]])

    def release(selection="desktop", tag="", results=(), git=None, gh=True):
        calls.clear(); sleeps.clear()
        run, git_output = fake(results, git)
        errors = io.StringIO()
        with patch.object(wena.shutil, "which", lambda name: "/bin/gh" if gh else None), \
                patch.object(wena, "git_output", git_output), patch.object(sys, "stderr", errors), \
                redirect_stdout(io.StringIO()):
            status = wena.release(selection, tag, ROOT, run, sleeps.append)
        return status, [c for c in calls if c[:3] == ["gh", "workflow", "run"]], errors.getvalue()

    status, runs, _ = release()
    assert status == 0
    assert runs == [["gh", "workflow", "run", "release-desktop.yml", "-R", "wekan/wena", "--ref", "main"]]
    status, runs, _ = release("all", "v1.2")
    assert status == 0 and [r[3] for r in runs] == ["release-desktop.yml", "release-all.yml"]
    assert runs[0][-2:] == ["-f", "tag=v1.2"] and "-f" not in runs[1]
    status, runs, _ = release("bootstrap")
    assert status == 0 and [r[3] for r in runs] == ["release-all.yml"]
    # Retried, then given up with how to fix it.
    status, runs, _ = release(results=[1, 1, 0])
    assert status == 0 and len(runs) == 3 and sleeps == [5, 5]
    status, runs, errors = release(results=[1, 1, 1])
    assert status == 1 and len(runs) == 3 and "workflow scope" in errors
    # Negative: nothing is started for any of these.
    for kwargs, code, message in [
            ({"selection": "everything"}, 2, "unknown release selection"),
            ({"tag": "v1;rm"}, 2, "invalid release tag"),
            ({"gh": False}, 1, "GitHub CLI"),
            ({"results": ["auth-fail"]}, 1, "gh auth login"),
            ({"git": {"remote": "/srv/git/wena.git"}}, 1, "GitHub origin"),
            ({"git": {"rev-parse": "HEAD"}}, 1, "GitHub origin"),
            ({"git": {"rev-list": None}}, 1, "Push it first"),
            ({"git": {"rev-list": "2"}}, 1, "2 local commit(s) are not on GitHub")]:
        status, runs, errors = release(**kwargs)
        assert (status, runs) == (code, []), kwargs
        assert message in errors, (kwargs, errors)
    assert "push origin main" in release(git={"rev-list": "2"})[2]
    # Release never pushes.
    release()
    assert not any(c[:2] == ["git", "push"] or "push" in c for c in calls)


def test_desktop_release_packaging():
    spec = importlib.util.spec_from_file_location("package_desktop_release", ROOT / "scripts" / "package_desktop_release.py")
    package = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(package)
    import hashlib, tarfile
    with tempfile.TemporaryDirectory() as temp:
        binary = Path(temp) / "wena-desktop"
        binary.write_bytes(b"\x7fELF fake desktop")
        first = package.package("linux-arm64", binary, Path(temp) / "a")
        second = package.package("linux-arm64", binary, Path(temp) / "b")
        assert first.name == "wena-desktop-linux-arm64.tar.gz"
        assert first.read_bytes() == second.read_bytes(), "not deterministic"
        sha = (first.parent / (first.name + ".sha256")).read_text()
        assert sha == hashlib.sha256(first.read_bytes()).hexdigest() + "  " + first.name + "\n"
        with tarfile.open(first) as archive:
            names = archive.getnames()
            members = {m.name: m for m in archive.getmembers()}
            sums = archive.extractfile("wena-desktop-linux-arm64/SHA256SUMS").read().decode()
            readme = archive.extractfile("wena-desktop-linux-arm64/README.txt").read().decode()
            for name in names[1:]:
                if not name.endswith("SHA256SUMS"):
                    data = archive.extractfile(name).read()
                    assert hashlib.sha256(data).hexdigest() + "  " + name.split("/", 1)[1] + "\n" in sums, name
        assert names[0] == "wena-desktop-linux-arm64"
        assert members["wena-desktop-linux-arm64/wena-desktop"].mode == 0o755
        assert {n.split("/", 1)[1] for n in names[1:]} == {"wena-desktop", "README.txt", "SHA256SUMS", *package.FILES}
        assert "libsdl2" in readme and "Linux arm64" in readme
        # Negative: an unknown platform, Windows among them, and an empty binary.
        for target in ("windows-amd64", "linux-i686"):
            try:
                package.package(target, binary, Path(temp) / "c")
            except ValueError:
                pass
            else:
                raise AssertionError(target)
        binary.write_bytes(b"")
        try:
            package.package("macos-arm64", binary, Path(temp) / "d")
        except ValueError:
            pass
        else:
            raise AssertionError("empty binary packaged")
    # The workflow builds, smoke-tests and packages exactly those platforms.
    workflow = (ROOT / ".github" / "workflows" / "release-desktop.yml").read_text(encoding="utf-8")
    targets = re.findall(r"- target: ([a-z0-9-]+)", workflow)
    assert sorted(targets) == sorted(package.TARGETS)
    assert "python3 scripts/wena.py build desktop" in workflow
    assert workflow.count("dist/desktop/wena-desktop --smoke") == 2
    assert 'package_desktop_release.py "$TARGET" dist/desktop/wena-desktop release-desktop' in workflow
    assert f'test "${{#assets[@]}}" -eq {2 * len(package.TARGETS)}' in workflow
    assert "workflow_dispatch:" in workflow and "tag:" in workflow
    assert "git push" not in workflow


def main():
    test_runner()
    test_release()
    test_desktop_release_packaging()
    test_run()
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
    for category in ("Build", "Run", "Tests", "Server", "Tools"):
        assert category in dispatcher


if __name__ == "__main__":
    main()
