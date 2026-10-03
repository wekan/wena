#!/usr/bin/env python3
"""The Nuklear desktop draws WeKan's board as WeKan does.

tests/fixtures/wekan-ui holds WeKan's own board UI, captured from a running
WeKan by tools/wekan-ui/capture.e2e.js and pinned by tools/wekan-ui/pin.py:
for each state (the board, its menus, card details, the sidebar) every
visible control with its text, tooltip, place and colors, and each surface's
computed colors. This checks Wena against it.
"""

import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / "tests" / "fixtures" / "wekan-ui"
# Wena is checked out in WeKan's .tools; WEKAN_ROOT names another checkout.
import os
WEKAN = Path(os.environ.get("WEKAN_ROOT") or ROOT.parents[1])


def rgb_hex(css):
    match = re.fullmatch(r"rgba?\((\d+), (\d+), (\d+)(?:, [\d.]+)?\)", css)
    assert match, css
    return "#%02x%02x%02x" % tuple(int(v) for v in match.groups())


def state(name):
    return json.loads((FIXTURES / f"{name}.json").read_text())


def wena_colors():
    """WenaWekanColor -> "#rrggbb", from the look module's own table."""
    header = (ROOT / "client/components/common/wekan_look.h").read_text()
    names = re.findall(r"^\s+WENA_WEKAN_([A-Z_]+),", header.split("typedef enum WenaWekanColor")[1].split("}")[0],
                       re.MULTILINE)
    source = (ROOT / "client/components/common/wekan_look.c").read_text()
    table = source.split("colors[WENA_WEKAN_COLOR_COUNT] = {")[1].split("};")[0]
    values = re.findall(r'"(#[0-9a-f]{6})"', table)
    assert len(names) == len(values), (names, values)
    return dict(zip(names, values))


def control_colors(data, region, **match):
    found = {rgb_hex(c["style"]["color"]) for c in data["controls"] if c["region"] == region and
             all(m in c["style"][k] for k, m in match.items())}
    assert found, (region, match)
    return found


def test_colors():
    colors = wena_colors()
    board, menu, card = state("01-board"), state("02-list-menu"), state("07-card-details")
    composer, sidebar = state("03-add-card"), state("06-sidebar")
    pixels = json.loads((FIXTURES / "pixels.json").read_text())["samples"]
    surface = lambda data, name, key="background": rgb_hex(data["surfaces"][name][key])
    expected = {
        "BODY": surface(board, "body"),
        "HEADER": surface(board, "header-quick-access"),
        "HEADER_TEXT": surface(board, "header-quick-access", "color"),
        "SWIMLANE_HEADER": surface(board, "swimlane-header"),
        "LIST": surface(board, "list"),
        "LIST_HEADER": surface(board, "list-header"),
        "LIST_BORDER": pixels["list-border"]["rgb"],
        "MINICARD": surface(board, "minicard"),
        "MINICARD_OPEN": surface(card, "minicard"),
        "MINICARD_TEXT": surface(board, "minicard-title", "color"),
        "MINICARD_SHADOW": pixels["minicard-shadow"]["rgb"],
        "TEXT": surface(board, "swimlane-header", "color"),
        "POPUP": surface(menu, "popup"),
        "POPUP_BORDER": surface(menu, "popup", "border"),
        "POPUP_HEADER": surface(menu, "popup-header"),
        "POPUP_HEADER_TEXT": surface(menu, "popup-header", "color"),
        "POPUP_HOVER": pixels["popup-hover"]["rgb"],
        "PANEL": surface(card, "card-details"),
        "BUTTON": surface(card, "button-primary"),
        "BUTTON_ADD": surface(composer, "button-primary"),
        "BUTTON_TEXT": surface(card, "button-primary", "color"),
        "INPUT_BORDER": surface(composer, "input", "border"),
    }
    for name, value in expected.items():
        assert colors[name] == value, f"{name}: Wena {colors[name]}, WeKan {value}"
    # The sidebar is the same panel color as card details.
    assert surface(sidebar, "sidebar") == colors["PANEL"]
    # Gray icons, header links, "+ Add Card" and section titles: their text colors.
    assert colors["ICON"] in control_colors(board, "list-header", font="Font Awesome")
    assert colors["HEADER_LINK"] in control_colors(board, "board-header")
    assert colors["ADD_CARD"] in control_colors(board, "composer")
    assert colors["SECTION_TITLE"] in control_colors(card, "card-details", font="700 16px")
    assert colors["ICON_ACTIVE"] in control_colors(menu, "list-header", font="Font Awesome")
    # Colors no captured state shows (a list over its WIP limit): WeKan's CSS.
    rules = json.loads((FIXTURES / "css.json").read_text())["rules"]
    for name, rule in rules.items():
        assert colors[name] == rule["value"], f"{name}: Wena {colors[name]}, WeKan {rule['value']}"
        stylesheet = WEKAN / rule["file"]
        if stylesheet.is_file():
            # Next to a WeKan checkout, the pin is checked against WeKan itself.
            block = re.search(re.escape(rule["selector"]) + r"\s*\{([^}]*)\}", stylesheet.read_text())
            assert block, (rule["file"], rule["selector"])
            value = re.search(rule["property"] + r"\s*:\s*(#[0-9a-fA-F]{6})", block.group(1))
            assert value and value.group(1).lower() == rule["value"], (rule, block.group(1))
    assert set(colors) == set(expected) | set(rules) | {"ICON", "HEADER_LINK", "ADD_CARD", "SECTION_TITLE",
                                                        "ICON_ACTIVE"}, \
        "every Wena color is checked against WeKan"


def main():
    test_colors()
    print("wekan-ui-parity: colors match WeKan's capture")


if __name__ == "__main__":
    main()
