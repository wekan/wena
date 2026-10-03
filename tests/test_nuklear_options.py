#!/usr/bin/env python3
"""Every Nuklear unit sees the same options, so struct nk_context has one layout.

The desktop crashed on macOS when the mouse reached a list or swimlane drag
handle: 42 sources included <nuklear.h> without the NK_INCLUDE_* options that
sdl_nuklear.h set before compiling the implementation, so reorder_drag.c read
context->current at another offset than Nuklear wrote it and got NULL. The
options now live only in client/platform/nuklear_options.h.
"""

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
OPTIONS = "client/platform/nuklear_options.h"
NUKLEAR = re.compile(r'^#include [<"](?:[./]*third_party/nuklear/)?nuklear\.h[>"]', re.M)


def sources():
    for folder in ("client", "tests"):
        for path in sorted((ROOT / folder).rglob("*")):
            if path.suffix in {".c", ".h"} and "fakes" not in path.parts:
                yield path


def check(path, text):
    """Problems in one file: an option set outside the header, or <nuklear.h>
    reached without the header first."""
    rel = path.relative_to(ROOT).as_posix()
    problems = []
    if rel != OPTIONS:
        for match in re.finditer(r'^#define (NK_INCLUDE_\w+|NK_INPUT_MAX)\b', text, re.M):
            problems.append(f"{rel}: defines {match.group(1)}; set it in {OPTIONS}")
    found = NUKLEAR.search(text)
    if found:
        options = text.find("nuklear_options.h")
        if options < 0 or options > found.start():
            problems.append(f"{rel}: includes nuklear.h without {OPTIONS} before it")
    return problems


def main():
    problems = []
    checked = 0
    for path in sources():
        text = path.read_text(encoding="utf-8", errors="replace")
        problems += check(path, text)
        checked += bool(NUKLEAR.search(text))
    assert checked >= 70, f"only {checked} Nuklear units found; the scan lost its way"
    # The guard catches both shapes of the fault.
    fake = ROOT / "client" / "x.c"
    assert check(fake, "#include <nuklear.h>\n")
    assert check(fake, '#define NK_INCLUDE_FONT_BAKING\n#include "platform/nuklear_options.h"\n#include <nuklear.h>\n')
    assert check(fake, '#include <nuklear.h>\n#include "platform/nuklear_options.h"\n')
    assert not check(fake, '#include "platform/nuklear_options.h"\n#include <nuklear.h>\n')
    if problems:
        print("\n".join(problems), file=sys.stderr)
        return 1
    print(f"nuklear options: {checked} units consistent")
    return 0


if __name__ == "__main__":
    sys.exit(main())
