#!/usr/bin/env python3
"""The one release workflow builds exactly the ready desktop targets.

.github/workflows/release-all.yml is the only workflow. Each ready target of
config/targets.tsv is built by exactly one job of the kind the catalog names,
uploaded under its own name, and attached beside one SHA256SUMS; nothing
else - no terminal-only program, no separate notices archive - is released.
"""

import importlib.util
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
WORKFLOWS = ROOT / ".github" / "workflows"
WORKFLOW = WORKFLOWS / "release-all.yml"


def module(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / "scripts" / f"{name}.py")
    loaded = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(loaded)
    return loaded


def job_block(workflow, job):
    """The text of one top-level job."""
    match = re.search(rf"^  {re.escape(job)}:\n(.*?)(?=^  [a-z][a-z-]*:\n|\Z)", workflow, re.MULTILINE | re.DOTALL)
    assert match, f"no {job} job"
    return match.group(1)


def matrix_targets(block):
    return re.findall(r"target: ([a-z0-9_-]+)", block)


def main() -> None:
    assert sorted(path.name for path in WORKFLOWS.iterdir()) == ["release-all.yml"], \
        "release-all.yml is the only workflow"
    workflow = WORKFLOW.read_text(encoding="utf-8")
    catalog = module("package_desktop_release").targets()
    ready = {target: record for target, record in catalog.items() if record[2] == "ready"}
    jobs = {"linux": "linux", "bsd": "bsd", "macos": "macos", "windows": "windows-build",
            "amiga": "amiga", "android": "android", "ios": "ios"}
    built = []
    for kind, job in jobs.items():
        wanted = [target for target, record in ready.items() if record[1] == kind]
        if not wanted:
            assert f"\n  {job}:\n" not in workflow, f"{job} job builds no ready target"
            continue
        block = job_block(workflow, job)
        found = matrix_targets(block)
        if job == "windows-build":
            found = re.findall(r"windows-[a-z0-9]+", re.search(r"target: \[([^\]]*)\]", block).group(1))
        assert sorted(found) == sorted(wanted), (job, found, wanted)
        built += found
        assert "name: wena-${{ matrix.target }}" in block, f"{job} uploads under another name"
    assert sorted(built) == sorted(ready), "every ready target is built exactly once"
    # Planned targets are not built yet.
    for target, record in catalog.items():
        if record[2] != "ready":
            assert f"target: {target}" not in workflow, target

    # The Linux containers are the ones a local build uses (scripts/toolchain.py).
    containers = module("toolchain").LINUX_CONTAINERS
    linux = job_block(workflow, "linux")
    for target, (platform, image) in containers.items():
        assert re.search(rf"target: {target}, runner: [a-z0-9.-]+, platform: {re.escape(platform)}, "
                         rf"image: '{re.escape(image)}'", linux), target
    # Regression (wena2 log): debian:bookworm lists neither armel nor mips64le.
    assert "platform: linux/arm/v5, image: 'arm32v5/debian:bookworm'" in linux
    assert "platform: linux/mips64le, image: 'mips64le/debian:bookworm'" in linux

    # BSDs and Haiku in their own virtual machines, pinned to an action release.
    bsd = job_block(workflow, "bsd")
    assert "uses: cross-platform-actions/action@v1.6.0" in bsd
    assert "sh scripts/build_desktop_release_vm.sh ${{ matrix.target }}" in bsd

    # Attach: every job, checksums, upload, and a failure naming what is missing.
    attach = job_block(workflow, "attach")
    for job in ["prepare", *[jobs[kind] for kind in {record[1] for record in ready.values()}]]:
        assert re.search(rf"needs: \[[^\]]*\b{re.escape(job)}\b", attach), job
    assert "package_desktop_release.py sums release" in attach
    assert "sha256sum -c SHA256SUMS" in attach
    assert "package_desktop_release.py missing release" in attach
    assert "--clobber" in attach

    # Negative: nothing but the desktop executables and SHA256SUMS is released.
    for gone in ("notices", "wena-desktop-", "release-desktop", ".sha256 ", "collect_release_assets",
                 "verify_release_assets", ".github/release/", "client/main.c"):
        assert gone not in workflow, gone
    assert not (ROOT / ".github" / "release").exists()
    # The X11 smoke test checks the application's own exit status.
    container = (ROOT / "scripts" / "build_desktop_release_container.sh").read_text(encoding="utf-8")
    assert "test \"$(cat \"$smoke/status\")\" = 0" in container
    # Only MIPS64, where Mesa's DRI driver crashes under QEMU, skips it; the
    # other CPUs keep the default renderer and Mesa's own driver search.
    assert ('if test "$target" = linux-mips64le; then\n'
            '  export LIBGL_DRIVERS_PATH=/nonexistent SDL_RENDER_DRIVER=software\nfi') in container
    assert container.count("LIBGL_DRIVERS_PATH") == 1 and container.count("SDL_RENDER_DRIVER") == 1


if __name__ == "__main__":
    main()
