#!/usr/bin/env python3
"""Pin a WeKan capture (tools/wekan-ui/capture.e2e.js) as Wena's reference.

    tools/wekan-ui/pin.py CAPTURE_DIR

Copies each STATE.json into tests/fixtures/wekan-ui/, with what changes from
run to run - the seeded board's random title, the user's name and initials,
ids in paths - replaced by fixed words, so the fixtures change only when
WeKan's UI does.
"""
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "tests" / "fixtures" / "wekan-ui"


def normalize(text):
    if not isinstance(text, str):
        return text
    text = re.sub(r"E2E Board board[0-9a-f]+", "E2E Board", text)
    text = re.sub(r"/b/[A-Za-z0-9]+/[a-z0-9-]+", "/b/BOARD/e2e-board", text)
    text = text.replace("ETU E2E Test User", "E2E Test User")
    # The seeded user's avatar initials.
    return re.sub(r"\bETU\b", "E2E", text)


def main(argv):
    if len(argv) != 1:
        print(__doc__, file=sys.stderr)
        return 2
    OUT.mkdir(parents=True, exist_ok=True)
    pinned = 0
    for path in sorted(Path(argv[0]).glob("*.json")):
        data = json.loads(path.read_text())
        data["url"] = normalize(data["url"])
        for control in data["controls"]:
            control["text"] = normalize(control["text"])
            control["title"] = normalize(control["title"])
        (OUT / path.name).write_text(json.dumps(data, indent=1, sort_keys=True) + "\n")
        pinned += 1
    print(f"pinned {pinned} WeKan UI states in {OUT.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
