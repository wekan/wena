#!/usr/bin/env python3
"""scripts/attach_release_files.sh, which each release build job runs the
moment its own file is built: every file of the directory goes to the
release in one upload, replacing one of the same name; SHA256SUMS is left to
the workflow's last job; a failed upload is tried three times; wrong use
attaches nothing. Run with a fake gh that records what it was asked."""
import os
from pathlib import Path
import stat
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "attach_release_files.sh"

FAKE_GH = """#!/bin/sh
echo "$*" >> "$GH_CALLS"
count=$(wc -l < "$GH_CALLS")
[ "$count" -gt "${GH_FAILS:-0}" ]
"""


def run(arguments, work, fails=0, repository="wekan/wena"):
    calls = work / "calls.txt"
    if calls.exists():
        calls.unlink()
    env = dict(os.environ, PATH=f"{work / 'bin'}:{os.environ['PATH']}", GH_CALLS=str(calls),
               GH_FAILS=str(fails), GITHUB_REPOSITORY=repository)
    # The retries wait 10 seconds each; a test need not.
    env["PATH"] = f"{work / 'bin'}:" + env["PATH"]
    result = subprocess.run(["sh", str(SCRIPT), *arguments], env=env, capture_output=True, text=True, timeout=120)
    return result, (calls.read_text().splitlines() if calls.exists() else [])


def main():
    with tempfile.TemporaryDirectory(dir=os.environ.get("TMPDIR")) as temp:
        work = Path(temp)
        (work / "bin").mkdir()
        for name, text in (("gh", FAKE_GH), ("sleep", "#!/bin/sh\nexit 0\n")):
            tool = work / "bin" / name
            tool.write_text(text)
            tool.chmod(tool.stat().st_mode | stat.S_IXUSR)
        release = work / "release"
        release.mkdir()
        (release / "wena-haiku-amd64").write_bytes(b"x")
        (release / "SHA256SUMS").write_text("y  wena-haiku-amd64\n")
        # One upload of the job's own file, replacing, never SHA256SUMS.
        result, calls = run(["v0.07", str(release)], work)
        assert result.returncode == 0, result.stderr
        assert calls == [f"release upload --repo wekan/wena v0.07 {release}/wena-haiku-amd64 --clobber"], calls
        assert "attached wena-haiku-amd64 to v0.07" in result.stdout
        # Tried again after a failure, and given up after three.
        result, calls = run(["v0.07", str(release)], work, fails=2)
        assert result.returncode == 0 and len(calls) == 3, (calls, result.stdout)
        result, calls = run(["v0.07", str(release)], work, fails=5)
        assert result.returncode == 1 and len(calls) == 3 and "Could not attach" in result.stdout
        # Negative: no tag, no directory, an empty one, and no repository.
        for arguments in (["", str(release)], ["v0.07", str(work / "none")], ["v0.07"]):
            result, calls = run(arguments, work)
            assert result.returncode == 2 and calls == [], arguments
        empty = work / "empty"
        empty.mkdir()
        (empty / "SHA256SUMS").write_text("")
        result, calls = run(["v0.07", str(empty)], work)
        assert result.returncode == 1 and calls == [] and "nothing to attach" in result.stderr
        result, calls = run(["v0.07", str(release)], work, repository="")
        assert result.returncode != 0 and calls == []
    print("attach_release_files.sh: each file at once, SHA256SUMS left out, retried, wrong use refused")


if __name__ == "__main__":
    main()
