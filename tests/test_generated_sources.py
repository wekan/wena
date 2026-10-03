#!/usr/bin/env python3
"""Every generated source matches what generates it.

The desktop build runs scripts/check_desktop_sources.sh first and stops on a
stale file - which is how the v0.03 release failed on every platform:
config/release-dependencies.json had changed (aros-x86 became aros-amd64)
and client/platform/notices_data.h, which compiles it in, had not been
regenerated. Running the same checks here makes a stale file fail the test
run, not the release.
"""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    check = subprocess.run(["sh", str(ROOT / "scripts" / "check_desktop_sources.sh")],
                           capture_output=True, text=True)
    assert check.returncode == 0, check.stdout + check.stderr
    # Negative: a notices header that no longer matches its sources fails, and
    # says what to run. Checked on a copy, so the checkout is never touched.
    with tempfile.TemporaryDirectory() as temp:
        copy = Path(temp) / "wena"
        (copy / "scripts").mkdir(parents=True)
        shutil.copy2(ROOT / "scripts" / "generate_notices.py", copy / "scripts")
        import importlib.util
        spec = importlib.util.spec_from_file_location("notices", ROOT / "scripts" / "generate_notices.py")
        notices = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(notices)
        for _title, source in notices.NOTICES:
            (copy / source).parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / source, copy / source)
        (copy / "client" / "platform").mkdir(parents=True)
        shutil.copy2(ROOT / "client" / "platform" / "notices_data.h", copy / "client" / "platform")
        fresh = subprocess.run([sys.executable, str(copy / "scripts" / "generate_notices.py"), "--check"],
                               capture_output=True, text=True)
        assert fresh.returncode == 0, fresh.stdout + fresh.stderr
        dependencies = copy / "config" / "release-dependencies.json"
        dependencies.write_text(dependencies.read_text(encoding="utf-8").replace("aros-amd64", "aros-x86"),
                                encoding="utf-8")
        stale = subprocess.run([sys.executable, str(copy / "scripts" / "generate_notices.py"), "--check"],
                               capture_output=True, text=True)
        assert stale.returncode != 0 and "notices_data.h is stale" in stale.stdout + stale.stderr
    print("generated sources: notices, translations, font, SVGs and migrations match their sources")


if __name__ == "__main__":
    main()
