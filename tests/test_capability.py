#!/usr/bin/env python3
"""Static fail-closed contract checks for the embedded capability asset."""

from pathlib import Path
import subprocess
import sys


script = Path(sys.argv[1]).read_text(encoding="utf-8")
assert "userAgent" not in script and "navigator" not in script
for required in (
    "'draggable' in s", "window.DragEvent", "window.DataTransfer",
    "window.AbortController", "window.fetch", "window.TextDecoder",
    "new window.DragEvent",
    "dispatchEvent(e)", "getElementsByClassName('wena-move-baseline')",
    "'-baseline'", "+'-drag'", "if(!drag){continue;}",
    "drag.focus()", "setAttribute('role','button')",
    "setAttribute('tabindex','0')", "aria-describedby", "aria-hidden",
    "style.display='none'", "function restore()", "style.display=x.display",
    "wena:enhancement-failed", "function guard(promise,milliseconds)",
    "window.setTimeout", "enhancement timeout", "restore();reject",
    "new Uint8Array(buffer)", "new window.TextDecoder('utf-8',{fatal:true})",
    "function safeNumber(value)", "n>9007199254740991",
    "b.length>32768", "WENA-REGIONS/1", "count>=8", "length>4096",
    "Object.prototype.hasOwnProperty.call(state.visible,r.name)",
    "Object.prototype.hasOwnProperty.call(names,match[1])",
    "parsed.requestVersion!==expected", "r.version<=current.version",
    "doc.getElementById(current.elementId)", "changes.push",
    "node.textContent", "state.lastRequestVersion=parsed.requestVersion",
    "catch(error){restore();throw error;}",
):
    assert required in script, required
assert "eval(" not in script and "innerHTML" not in script
assert script.count("style.display='none'") == 1
