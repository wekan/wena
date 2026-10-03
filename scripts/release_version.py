#!/usr/bin/env python3
"""Wena release numbers and notes, from CHANGELOG.md.

Versions follow WeKan's: v<MAJOR>.<MINOR, two digits>, one step 0.01 at a time
(v9.99 is followed by v10.00); the first Wena release is v0.01. The next
version is one step after the highest of the published releases and the
released CHANGELOG sections.

    release_version.py next [PUBLISHED_TAG...]   -> the next version
    release_version.py notes VERSION             -> that section's text
    release_version.py upcoming                  -> exit 0 when Upcoming has entries
"""

import datetime
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
UPCOMING = "# Upcoming Wena release"
VERSION = re.compile(r"v(\d+)\.(\d{2})")
HEADING = re.compile(r"^# (v\d+\.\d{2}) \d{4}-\d{2}-\d{2} Wena release$", re.M)


def parse(version):
    match = VERSION.fullmatch(version or "")
    return (int(match[1]), int(match[2])) if match else None


def next_version(published, changelog):
    known = [parse(tag) for tag in list(published) + HEADING.findall(changelog)]
    known = [value for value in known if value is not None]
    if not known:
        return "v0.01"
    major, minor = max(known)
    major, minor = (major + 1, 0) if minor == 99 else (major, minor + 1)
    return f"v{major}.{minor:02d}"


def upcoming_entries(changelog):
    """The Upcoming section's text; ValueError when it is missing, repeated or empty."""
    if changelog.splitlines().count(UPCOMING) != 1:
        raise ValueError(f"CHANGELOG.md must have exactly one '{UPCOMING}' heading")
    body = re.split(r"^# ", changelog.split(UPCOMING + "\n", 1)[1], maxsplit=1, flags=re.M)[0]
    if not re.search(r"<summary>|^- \S", body, re.M):
        raise ValueError("Upcoming has no entries to release")
    return body


def name_release(changelog, version, date=None):
    """Rename Upcoming to the version's heading."""
    if parse(version) is None:
        raise ValueError(f"invalid version {version}")
    upcoming_entries(changelog)
    date = date or datetime.date.today().isoformat()
    return changelog.replace(UPCOMING, f"# {version} {date} Wena release", 1)


def notes(changelog, version):
    for match in HEADING.finditer(changelog):
        if match[1] == version:
            body = changelog[match.end():]
            following = re.search(r"^# ", body, re.M)
            return (body[:following.start()] if following else body).strip() + "\n"
    raise ValueError(f"CHANGELOG.md has no section for {version}")


def main(argv):
    changelog = (ROOT / "CHANGELOG.md").read_text(encoding="utf-8")
    try:
        if argv[:1] == ["next"]:
            print(next_version(argv[1:], changelog))
        elif len(argv) == 2 and argv[0] == "notes":
            sys.stdout.write(notes(changelog, argv[1]))
        elif argv == ["upcoming"]:
            upcoming_entries(changelog)
        else:
            print(__doc__, file=sys.stderr)
            return 2
    except ValueError as error:
        print(f"release_version: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
