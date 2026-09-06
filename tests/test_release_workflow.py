#!/usr/bin/env python3
"""Structural regression checks for the release target matrix."""

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
WORKFLOW = ROOT / ".github" / "workflows" / "release-all.yml"
ROADMAP = ROOT / "ROADMAP.md"


def listed_targets(text: str) -> list[str]:
    return re.findall(r"^\s+- target: ([a-z0-9-]+)$", text, re.MULTILINE)


def main() -> None:
    workflow = WORKFLOW.read_text(encoding="utf-8")
    roadmap = ROADMAP.read_text(encoding="utf-8")
    expected = [
        "linux-arm64",
        "linux-amd64",
        "linux-armhf",
        "windows-amd64",
        "macos-arm64",
        "macos-amd64",
        "amigaos-m68k",
        "aros-x86",
        "android-arm64",
        "ios-arm64",
    ]

    assert listed_targets(workflow) == expected, "release targets changed or reordered"
    assert workflow.count("name: ") >= len(expected), "every target needs a display name"
    assert ".github/release/${TARGET}.sh" in workflow
    assert "dist/${{ matrix.target }}/" in workflow
    assert "ready target is missing" in workflow
    assert "exists but is not executable" in workflow
    assert "steps.support.outputs.enabled" not in workflow
    assert "if-no-files-found: error" in workflow
    assert "permissions:\n  contents: read" in workflow
    assert "https://api.github.com/repos/wekan/wena/releases/latest" in workflow
    assert "--request GET" in workflow
    assert "needs: resolve-release" in workflow
    assert "release_id: ${{ steps.release.outputs.release_id }}" in workflow
    assert "release_tag: ${{ steps.release.outputs.release_tag }}" in workflow
    assert "latest Wena release response lacks id or tag_name" in workflow
    assert "--request POST" not in workflow
    assert "--request PATCH" not in workflow
    assert "gh release" not in workflow
    assert "action-gh-release" not in workflow
    completed = {
        "Linux arm64", "Linux amd64", "Linux armhf", "Windows amd64",
        "macOS arm64", "macOS amd64", "AmigaOS 3.x m68k", "AROS x86",
        "Android arm64", "iOS arm64",
    }
    for display_name in (
        "Linux arm64", "Linux amd64", "Linux armhf", "Windows amd64",
        "macOS arm64", "macOS amd64", "AmigaOS 3.x m68k", "AROS x86",
        "Android arm64", "iOS arm64",
    ):
        marker = "x" if display_name in completed else "_"
        assert f"- [{marker}] {display_name}" in roadmap


if __name__ == "__main__":
    main()
