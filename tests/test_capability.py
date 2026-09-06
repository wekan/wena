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
    "window.FormData", "window.URLSearchParams",
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
    "function postForm(form,state,doc,milliseconds)",
    "method!=='POST'", "!sameOrigin(action)", "state.inFlight",
    "new window.AbortController()", "new window.FormData(form)",
    "new window.URLSearchParams", "credentials:'same-origin'",
    "redirect:'error'", "application/vnd.wena.regions-v1",
    "application/x-www-form-urlencoded;charset=UTF-8",
    "X-Wena-Request-Version", "signal:controller.signal",
    "response.ok", "response.arrayBuffer()", "buffer.byteLength>32768",
    "applyRegions(buffer,version,state,doc)", "controller.abort()",
    "state.inFlight=false", "state.nextRequestVersion=version+1",
    "function signedMoveForm(doc,id)", "String(form.tagName).toUpperCase()!=='FORM'",
    "form.className.indexOf('wena-move-baseline')<0",
    "field(form,'legacySession')", "field(form,'csrf')",
    "field(form,'legacyOperation')", "function bindDragDrop(doc,state,milliseconds)",
    "getElementsByClassName('wena-drag-source')",
    "getElementsByClassName('wena-drop-target')", "data-wena-form",
    "setData('application/x-wena-move','1')",
    "getData('application/x-wena-move')==='1'", "postForm(form,state,doc,milliseconds)",
    "event.key==='Enter'||event.key===' '", "Move failed; use move buttons.",
    "Move selected; choose a destination.",
    "live.textContent='Move complete.'", "if(target.focus){target.focus();}",
    "if(!activate(doc)){return false;}", "if(!bindDragDrop(doc,state,5000)){restore();return false;}",
):
    assert required in script, required
assert "eval(" not in script and "innerHTML" not in script
assert "userAgent" not in script
assert script.count("style.display='none'") == 1
assert "doc.body.className+=' wena-enhanced'" in script
assert "replace(/ ?wena-enhanced/g,'')" in script
