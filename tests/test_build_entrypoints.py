#!/usr/bin/env python3
"""Regression checks for shared interactive and noninteractive build dispatch."""

from contextlib import redirect_stdout
import importlib.util
import io
import json
import sys
import threading
import time
from unittest.mock import patch
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("wena_commands", ROOT / "scripts" / "wena.py")
wena = importlib.util.module_from_spec(spec)
spec.loader.exec_module(wena)


def command(*arguments):
    return subprocess.run([str(ROOT / "build.sh"), *arguments], text=True, capture_output=True)


class FakePlan:
    def __init__(self, env=None, commands=None):
        self.env = env or {}
        self.commands = commands or []

    def environment(self):
        return dict(self.env)


def test_runner():
    # What a build needs is installed first (scripts/toolchain.py, tested in
    # test_toolchain.py); here nothing is installed.
    with patch.object(wena, "prepare_toolchain", return_value=FakePlan({"WENA_CC": "gcc"})) as prepared, \
            patch.object(wena.subprocess, "call", return_value=0) as call:
        with redirect_stdout(io.StringIO()):
            assert wena.build("desktop") == 0
        prepared.assert_called_once_with("desktop")
        assert call.call_args.kwargs["env"] == {"WENA_CC": "gcc"}
        assert call.call_args.args[0][-2] == str(ROOT / "scripts" / "build_desktop.sh")
        assert Path(call.call_args.args[0][-1]).parent == ROOT / "dist" / "desktop"
    with patch.object(wena.shutil, "which", return_value=None):
        assert "SDL2" in wena.test_prerequisite("desktop")
    # Negative: a desktop build whose requirements cannot be installed runs nothing.
    with patch.object(wena, "prepare_toolchain", return_value=None), \
            patch.object(wena.subprocess, "call") as call, redirect_stdout(io.StringIO()):
        assert wena.build("desktop") == 1
    call.assert_not_called()
    # All: each target runs its plan's commands in order; one this computer
    # cannot build is listed and skipped, and a failing one fails the build
    # without stopping the others.
    ready = [item["target"] for item in wena.targets() if item["status"] == "ready"]
    unavailable = ready[-1]
    plans = {target: FakePlan({"T": target}, [["build", target], ["package", target]]) for target in ready}
    plans[unavailable] = None
    with patch.object(wena, "prepare_toolchain", lambda target: plans.get(target)), \
            patch.object(wena.subprocess, "call", side_effect=lambda command, **kwargs:
                         3 if command[-1] == "windows-amd64" and command[0].endswith("build") else 0) as call, \
            redirect_stdout(io.StringIO()) as printed:
        assert wena.build("all") == 1
    builds = [c.args[0] for c in call.call_args_list if c.args[0][0].endswith(("build", "package"))]
    assert ["build", "linux-amd64"] in builds and ["package", "linux-amd64"] in builds
    # A failed build is not packaged.
    assert ["build", "windows-amd64"] in builds and ["package", "windows-amd64"] not in builds
    assert not any(command[-1] == unavailable for command in builds)
    assert f"Not buildable on this computer: {unavailable}" in printed.getvalue()
    assert "Failed: windows-amd64" in printed.getvalue()
    assert len(builds) == 2 * (len(ready) - 1) - 1
    environments = [c.kwargs["env"] for c in call.call_args_list if c.args[0][0].endswith("build")]
    assert {"T": "linux-amd64"} in environments
    # One target that cannot be built here is a failure, and runs nothing.
    with patch.object(wena, "prepare_toolchain", return_value=None), \
            patch.object(wena.subprocess, "call", return_value=0) as call, redirect_stdout(io.StringIO()):
        assert wena.build(unavailable) == 1
        assert wena.install(unavailable) == 1
    assert not any(c.args[0][0].endswith(("build", "package")) for c in call.call_args_list)
    # Negative: a planned target is refused before anything runs.
    planned = [item["target"] for item in wena.targets() if item["status"] != "ready"]
    if planned:
        with patch.object(wena, "prepare_toolchain") as prepared:
            try:
                wena.build(planned[0])
            except SystemExit as error:
                assert "not ready" in str(error)
            else:
                raise AssertionError("a planned target was built")
        prepared.assert_not_called()
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
    records = [(name, "", "") for name in ("first", "second", "third", "desktop")]
    active = 0
    maximum = 0
    lock = threading.Lock()
    def fake(record):
        nonlocal active, maximum
        with lock:
            if record[0] == "desktop":
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
    def __init__(self, returncode, stdout=""):
        self.returncode = returncode
        self.stdout = stdout


def test_release():
    # 3) Release: the human's step. gh, git and GitHub are replaced here, so
    # nothing is pushed or started; a real git repository holds the CHANGELOG.
    if shutil.which("git") is None:
        print("SKIP release flow: needs git")
        return
    assert wena.github_repository("git@github.com:wekan/wena") == "wekan/wena"
    assert wena.github_repository("https://github.com/wekan/wena.git") == "wekan/wena"
    assert wena.github_repository("/srv/git/wena.git") is None
    assert wena.github_repository("https://github.com/wekan") is None
    calls, sleeps = [], []

    def release(mode="next", published="", gh=True, auth=0, workflow=(), push=0, git=None, changelog=None):
        calls.clear(); sleeps.clear()
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            subprocess.run(["git", "init", "-q", str(root)], check=True)
            for key, value in (("user.name", "Test"), ("user.email", "t@example.org")):
                subprocess.run(["git", "-C", str(root), "config", key, value], check=True)
            (root / "CHANGELOG.md").write_text(changelog if changelog is not None else
                "# Upcoming Wena release\n\n- Something new.\n\n# v0.01 2026-01-01 Wena release\n\n- First.\n",
                encoding="utf-8")
            (root / "scripts").mkdir()
            (root / "scripts" / "release_version.py").write_bytes((ROOT / "scripts" / "release_version.py").read_bytes())
            subprocess.run(["git", "-C", str(root), "add", "-A"], check=True)
            subprocess.run(["git", "-C", str(root), "commit", "-q", "-m", "start"], check=True)
            answers = {"remote": "git@github.com:wekan/wena", "rev-parse": "main", "status": "", **(git or {})}
            results = list(workflow)

            def run(command, **kwargs):
                calls.append(command)
                if command[:3] == ["gh", "auth", "status"]:
                    return Completed(auth)
                if command[:3] == ["gh", "release", "list"]:
                    return Completed(0, published)
                if command[:3] == ["gh", "workflow", "run"]:
                    return Completed(results.pop(0) if results else 0)
                if command[3:4] == ["push"]:
                    return Completed(push)
                return subprocess.run(command, capture_output=True)

            errors = io.StringIO()
            with patch.object(wena.shutil, "which", lambda name: "/bin/gh" if gh else None), \
                    patch.object(wena, "git_output", lambda *a, root=None: answers[a[0]]), \
                    patch.object(sys, "stderr", errors), redirect_stdout(io.StringIO()):
                status = wena.release(mode, root, run, sleeps.append, "2026-10-03")
            log = subprocess.run(["git", "-C", str(root), "log", "--format=%s"], capture_output=True, text=True).stdout
            text = (root / "CHANGELOG.md").read_text(encoding="utf-8")
        runs = [c for c in calls if c[:3] == ["gh", "workflow", "run"]]
        pushes = [c for c in calls if c[3:4] == ["push"]]
        return status, runs, pushes, log, text, errors.getvalue()

    # Next version: one step after the highest released one, here or on GitHub.
    status, runs, pushes, log, text, _ = release()
    assert status == 0
    assert text.startswith("# v0.02 2026-10-03 Wena release\n") and "# Upcoming" not in text
    assert log.splitlines()[0] == "Prepare v0.02 release"
    assert pushes and runs == [["gh", "workflow", "run", "release-all.yml", "-R", "wekan/wena",
                                "--ref", "main", "-f", "version=v0.02"]]
    assert release(published="v0.09\nv0.05\n")[4].startswith("# v0.10 ")
    assert release(published="v9.99\n")[4].startswith("# v10.00 ")
    # Build missing files: no commit, no version.
    status, runs, pushes, log, text, _ = release("missing")
    assert status == 0 and runs[0][-2:] == ["--ref", "main"] and log.strip() == "start"
    assert text.startswith("# Upcoming Wena release")
    # Retried, then given up with how to fix it.
    assert release(workflow=[1, 1, 0])[0] == 0 and sleeps == [5, 5]
    status, runs, _, _, _, errors = release("missing", workflow=[1, 1, 1])
    assert status == 1 and len(runs) == 3 and "workflow scope" in errors
    # Negative: nothing is committed, pushed or started for any of these.
    for kwargs, code, message in [
            ({"mode": "everything"}, 2, "unknown release mode"),
            ({"gh": False}, 1, "GitHub CLI"),
            ({"auth": 1}, 1, "gh auth login"),
            ({"git": {"remote": "/srv/git/wena.git"}}, 1, "GitHub origin"),
            ({"git": {"rev-parse": "HEAD"}}, 1, "GitHub origin"),
            ({"git": {"status": " M client/desktop.c"}}, 1, "Commit or stash"),
            ({"changelog": "# v0.01 2026-01-01 Wena release\n\n- First.\n"}, 1, "Upcoming"),
            ({"changelog": "# Upcoming Wena release\n\nNothing yet.\n"}, 1, "no entries")]:
        status, runs, pushes, log, text, errors = release(**kwargs)
        assert (status, runs, pushes, log.strip()) == (code, [], [], "start"), kwargs
        assert message in errors, (kwargs, errors)
    # A failed push starts nothing.
    status, runs, _, _, _, errors = release("missing", push=1)
    assert status == 1 and runs == [] and "Push failed" in errors


def test_release_versions():
    versions = wena.release_version_module()
    log = "# Upcoming Wena release\n\n<details>\n<summary>New</summary>\n</details>\n\n# v1.04 2026-09-01 Wena release\n\n- Old.\n\n# v1.03 2026-08-01 Wena release\n\n- Older.\n"
    assert versions.next_version([], "# Upcoming Wena release\n\n- x\n") == "v0.01"
    assert versions.next_version([], log) == "v1.05"
    assert versions.next_version(["v1.07", "nightly", "v2", "v1.5"], log) == "v1.08"
    assert versions.next_version(["v3.99"], "") == "v4.00"
    renamed = versions.name_release(log, "v1.05", "2026-10-03")
    assert renamed.startswith("# v1.05 2026-10-03 Wena release\n") and "Upcoming" not in renamed
    assert versions.notes(renamed, "v1.05") == "<details>\n<summary>New</summary>\n</details>\n"
    assert versions.notes(renamed, "v1.04") == "- Old.\n"
    for bad in (lambda: versions.name_release(log, "1.05"), lambda: versions.notes(log, "v9.99"),
                lambda: versions.upcoming_entries(log + "# Upcoming Wena release\n- x\n"),
                lambda: versions.upcoming_entries("# Upcoming Wena release\n\n# v1.00 2026-01-01 Wena release\n- x\n")):
        try:
            bad()
        except ValueError:
            pass
        else:
            raise AssertionError("accepted")


def elf(machine, needed, bits64=True, little=True):
    """A minimal ELF with a dynamic section naming `needed`."""
    import struct
    e = "<" if little else ">"
    strtab = b"\0" + b"".join(n.encode() + b"\0" for n in needed)
    offsets, pos = [], 1
    for n in needed:
        offsets.append(pos); pos += len(n) + 1
    if bits64:
        header_size, ph_size, dyn_size = 64, 56, 16
    else:
        header_size, ph_size, dyn_size = 52, 32, 8
    phoff = header_size
    dynoff = phoff + 2 * ph_size
    dyn = [(1, o) for o in offsets] + [(5, 0), (0, 0)]
    stroff = dynoff + len(dyn) * dyn_size
    data = bytearray(stroff + len(strtab))
    data[0:6] = b"\x7fELF" + bytes([2 if bits64 else 1, 1 if little else 2])
    struct.pack_into(e + "H", data, 18, machine)
    if bits64:
        struct.pack_into(e + "Q", data, 32, phoff); struct.pack_into(e + "HH", data, 54, ph_size, 2)
        struct.pack_into(e + "IIQQQQ", data, phoff, 1, 5, 0, 0, 0, len(data))
        struct.pack_into(e + "IIQQQQ", data, phoff + ph_size, 2, 6, dynoff, dynoff, dynoff, len(dyn) * dyn_size)
        for i, (tag, value) in enumerate(dyn):
            struct.pack_into(e + "qQ", data, dynoff + i * dyn_size, tag, stroff if tag == 5 else value)
    else:
        struct.pack_into(e + "I", data, 28, phoff); struct.pack_into(e + "HH", data, 42, ph_size, 2)
        struct.pack_into(e + "IIIII", data, phoff, 1, 0, 0, 0, len(data))
        struct.pack_into(e + "IIIII", data, phoff + ph_size, 2, dynoff, dynoff, dynoff, len(dyn) * dyn_size)
        for i, (tag, value) in enumerate(dyn):
            struct.pack_into(e + "iI", data, dynoff + i * dyn_size, tag, stroff if tag == 5 else value)
    data[stroff:] = strtab
    return bytes(data)


def pe(machine, dlls):
    """A minimal PE32+ with an import table naming `dlls`."""
    import struct
    data = bytearray(0x400 + 20 * (len(dlls) + 1) + sum(len(d) + 1 for d in dlls))
    data[0:2] = b"MZ"; struct.pack_into("<I", data, 0x3C, 0x80)
    data[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<HHIIIHH", data, 0x84, machine, 1, 0, 0, 0, 240, 0)
    opt = 0x98
    struct.pack_into("<H", data, opt, 0x20B)
    struct.pack_into("<II", data, opt + 112 + 8, 0x1000, 20 * (len(dlls) + 1))
    struct.pack_into("<8sIIII", data, opt + 240, b".idata", len(data), 0x1000, len(data) - 0x400, 0x400)
    names = 0x400 + 20 * (len(dlls) + 1)
    for i, dll in enumerate(dlls):
        struct.pack_into("<I", data, 0x400 + 20 * i + 12, 0x1000 + names - 0x400)
        data[names:names + len(dll)] = dll.encode(); names += len(dll) + 1
    return bytes(data)


def macho(cpu, dylibs):
    import struct
    commands = b""
    for name in dylibs:
        body = name.encode() + b"\0"
        size = (24 + len(body) + 7) // 8 * 8
        commands += struct.pack("<IIIIII", 0xC, size, 24, 0, 0, 0) + body.ljust(size - 24, b"\0")
    return struct.pack("<IiiIIII", 0xFEEDFACF, cpu, 0, 2, len(dylibs), len(commands), 0) + b"\0" * 4 + commands


def test_release_executable_check():
    spec = importlib.util.spec_from_file_location("check", ROOT / "scripts" / "check_release_executable.py")
    check = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(check)
    assert check.check("linux-amd64", elf(62, ["libm.so.6", "libc.so.6"])) == ["libm.so.6", "libc.so.6"]
    assert check.check("linux-armhf", elf(40, ["libc.so.6"], bits64=False)) == ["libc.so.6"]
    assert check.check("linux-s390x", elf(22, ["libc.so.6"], little=False)) == ["libc.so.6"]
    assert check.check("windows-amd64", pe(0x8664, ["KERNEL32.dll", "USER32.dll", "api-ms-win-crt-heap-l1-1-0.dll"]))
    assert check.check("macos-arm64", macho(0x0100000C, ["/usr/lib/libSystem.B.dylib",
                                                          "/System/Library/Frameworks/Cocoa.framework/Versions/A/Cocoa"]))
    # Negative: the libraries that must be linked in, a MinGW runtime DLL,
    # a Homebrew dylib, and the wrong CPU or format for the target.
    for target, data, message in [
            ("linux-amd64", elf(62, ["libSDL2-2.0.so.0", "libc.so.6"]), "must be linked in"),
            ("linux-amd64", elf(62, ["libsqlite3.so.0"]), "must be linked in"),
            ("linux-arm64", elf(62, ["libc.so.6"]), "is not arm64"),
            ("windows-amd64", pe(0x8664, ["SDL2.dll"]), "not part of Windows"),
            ("windows-i686", pe(0x14C, ["libwinpthread-1.dll"]), "not part of Windows"),
            ("windows-arm64", pe(0x8664, ["KERNEL32.dll"]), "is not arm64"),
            ("macos-arm64", macho(0x0100000C, ["/opt/homebrew/opt/sdl2-compat/lib/libSDL2-2.0.0.dylib"]), "outside the system"),
            ("macos-amd64", macho(0x0100000C, []), "is not amd64"),
            ("linux-amd64", pe(0x8664, []), "not an ELF"),
            ("windows-amd64", elf(62, []), "not a PE"),
            ("plan9-amd64", elf(62, []), "unknown target")]:
        try:
            check.check(target, data)
        except ValueError as error:
            assert message in str(error), (target, error)
        else:
            raise AssertionError((target, message))


def test_desktop_release_packaging():
    spec = importlib.util.spec_from_file_location("package_desktop_release", ROOT / "scripts" / "package_desktop_release.py")
    package = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(package)
    import hashlib
    with tempfile.TemporaryDirectory() as temp:
        binary = Path(temp) / "wena"
        binary.write_bytes(b"\x7fELF fake desktop")
        out = Path(temp) / "out"
        # Each release file is named after its target, from the catalog.
        linux = package.binary("linux-riscv64", binary, out)
        windows = package.binary("windows-arm64", binary, out)
        aros = package.binary("aros-x86", binary, out) if "aros-x86" in package.targets() else None
        assert linux.name == "wena-linux-riscv64" and windows.name == "wena-windows-arm64.exe"
        assert aros is None or aros.name == "wena-aros-x86"
        for path in (linux, windows):
            assert path.read_bytes() == binary.read_bytes()
        # One SHA256SUMS for all of them, in sha256sum -c format, and no other file.
        sums = package.sums(out)
        lines = sums.read_text().splitlines()
        assert f"{hashlib.sha256(binary.read_bytes()).hexdigest()}  wena-linux-riscv64" in lines
        assert sorted(path.name for path in out.iterdir()) == sorted(
            [p.name for p in (linux, windows, aros) if p] + ["SHA256SUMS"])
        assert not list(out.glob("*.sha256")) and not list(out.glob("*notices*"))
        missing = package.missing(out)
        assert "linux-riscv64" not in missing and "linux-amd64" in missing
        # Negative: an unknown platform, an empty or missing executable, and a
        # stray file that is not any target's release file.
        binary.write_bytes(b"")
        for target, path in (("plan9-amd64", Path(temp) / "x"), ("linux-amd64", binary), ("linux-amd64", Path(temp) / "missing")):
            try:
                package.binary(target, path, out)
            except ValueError:
                pass
            else:
                raise AssertionError(target)
        (out / "wena-desktop-notices.tar.gz").write_bytes(b"old")
        try:
            package.sums(out)
        except ValueError as error:
            assert "not a release file" in str(error)
        else:
            raise AssertionError("a stray file was checksummed for release")
    workflow = (ROOT / ".github" / "workflows" / "release-all.yml").read_text(encoding="utf-8")
    assert "python3 scripts/release_version.py notes" in workflow and "gh release create" in workflow
    assert "git push" not in workflow
    # Every release dependency is pinned with a SHA-256.
    pins = json.loads((ROOT / "config" / "release-dependencies.json").read_text())
    for name, pin in pins.items():
        if isinstance(pin, dict):
            assert re.fullmatch(r"[0-9a-f]{64}", pin["sha256"]) and pin["url"].startswith("https://"), name


def test_release_dependency_fetch():
    spec = importlib.util.spec_from_file_location("fetch", ROOT / "scripts" / "fetch_release_dependency.py")
    fetch = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(fetch)
    import hashlib
    good = b"pinned source"
    pins = {"lib": {"url": "https://example.org/lib-1.tar.gz", "sha256": hashlib.sha256(good).hexdigest()}}
    downloads = []

    def download(body):
        def get(url, path):
            downloads.append(url)
            Path(path).write_bytes(body)
        return get
    with tempfile.TemporaryDirectory() as temp:
        path = fetch.fetch("lib", temp, pins, download(good))
        assert path.name == "lib-1.tar.gz" and path.read_bytes() == good
        # A matching cached file is used without downloading again.
        assert fetch.fetch("lib", temp, pins, download(b"other")) == path and len(downloads) == 1
        # Negative: a changed cached file is downloaded again; a download that
        # does not match is deleted and refused; an unknown name is refused.
        path.write_bytes(b"tampered")
        assert fetch.fetch("lib", temp, pins, download(good)).read_bytes() == good
        path.unlink()
        for name, body in (("lib", b"tampered download"), ("unknown", good), ("format", good)):
            try:
                fetch.fetch(name, temp, {**pins, "format": 1}, download(body))
            except ValueError:
                pass
            else:
                raise AssertionError(name)
        assert sorted(p.name for p in Path(temp).iterdir()) == []


def main():
    test_runner()
    test_release()
    test_release_versions()
    test_release_executable_check()
    test_release_dependency_fetch()
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

    not_ready = [item["target"] for item in wena.targets() if item["status"] != "ready"]
    if not_ready:
        planned = command("build", not_ready[0])
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
    # A target is built by the commands its toolchain plan names, not by a
    # per-target script; a planned one is refused before anything runs.
    assert "plan.commands" in dispatcher and "cataloged but not ready" in dispatcher
    assert "verify_i18n_catalog.py" in dispatcher
    for category in ("Build", "Run", "Tests", "Server", "Tools"):
        assert category in dispatcher


if __name__ == "__main__":
    main()
