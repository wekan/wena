#!/usr/bin/env python3
"""Static fail-closed contract checks for the embedded capability asset."""

from pathlib import Path
import subprocess
import sys


script = Path(sys.argv[1]).read_text(encoding="utf-8")
assert "userAgent" not in script and "navigator" not in script
for required in (
    "'draggable' in s", "window.DragEvent", "window.DataTransfer",
    "window.AbortController", "window.fetch", "new window.DragEvent",
    "dispatchEvent(e)", "getElementsByClassName('wena-move-baseline')",
    "'-baseline'", "+'-drag'", "if(!drag){continue;}",
    "drag.focus()", "setAttribute('role','button')",
    "setAttribute('tabindex','0')", "aria-describedby", "aria-hidden",
    "style.display='none'", "function restore()", "style.display=x.display",
    "wena:enhancement-failed", "function guard(promise,milliseconds)",
    "window.setTimeout", "enhancement timeout", "restore();reject",
):
    assert required in script, required
assert "eval(" not in script and "innerHTML" not in script
assert script.count("style.display='none'") == 1
