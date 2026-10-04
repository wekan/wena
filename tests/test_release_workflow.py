#!/usr/bin/env python3
"""The release workflow builds exactly the ready desktop targets.

.github/workflows/release-all.yml builds; release-all-missing.yml only calls
it for what a release lacks. Each ready target of
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
    assert sorted(path.name for path in WORKFLOWS.iterdir()) == ["release-all-missing.yml", "release-all.yml"], \
        "release-all.yml builds, release-all-missing.yml calls it"
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
        # Only the targets asked for, and each file attached as soon as its
        # own job has built and checked it - not when every job has finished.
        assert "WANTED: ${{ inputs.targets == '' || contains(format(' {0} ', inputs.targets), " \
               "format(' {0} ', matrix.target)) }}" in block, job
        steps = block[block.index("    steps:"):]
        assert steps.count("      - ") == steps.count("env.WANTED == 'true'"), f"{job}: an ungated step"
        attaching = job_block(workflow, "windows-smoke") if job == "windows-build" else block
        assert 'run: sh scripts/attach_release_files.sh "$TAG" release' in attaching, job
        assert "TAG: ${{ needs.prepare.outputs.tag }}" in attaching and "contents: write" in attaching, job
        assert attaching.rstrip().endswith('run: sh scripts/attach_release_files.sh "$TAG" release'), \
            f"{job}: attaching is not the last step"
        # A cancelled run still attaches what was built: the step runs after a
        # cancel, but only when its own build step succeeded.
        assert attaching.count("        id: build\n") == 1, f"{job}: no single build step"
        assert "if: ${{ always() && env.WANTED == 'true' && steps.build.outcome == 'success' }}" in attaching, job
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

    # Windows is attached after its smoke test on Windows, not by the cross build.
    assert "attach_release_files" not in job_block(workflow, "windows-build")
    assert "needs: [prepare, windows-build]" in job_block(workflow, "windows-smoke")
    # Callable with a version and targets, and startable with targets.
    assert re.search(r"workflow_call:\n    inputs:\n      version:.*\n(.*\n)*?      targets:", workflow)
    assert "targets:\n        description: 'Only these targets" in workflow
    # Checksums: after every job, over all the release's files, and a failure
    # naming what is missing.
    attach = job_block(workflow, "attach")
    assert "if: ${{ always() && needs.prepare.result == 'success' }}" in attach, "cancelled runs skip SHA256SUMS"
    # Negative: nothing that attaches or sums is skipped by a cancel.
    assert "!cancelled()" not in workflow
    assert "gh release download" in attach and "--pattern 'wena-*'" in attach
    assert "release/SHA256SUMS --clobber" in attach
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


def test_missing():
    """release-all-missing.yml builds what the newest (or a named) release
    lacks by calling release-all.yml with those targets."""
    missing = (WORKFLOWS / "release-all-missing.yml").read_text(encoding="utf-8")
    assert "package_desktop_release.py missing-from" in missing
    assert "gh release view \"$tag\" --repo \"$GITHUB_REPOSITORY\" --json assets --jq '.assets[].name'" in missing
    assert "uses: ./.github/workflows/release-all.yml" in missing and "secrets: inherit" in missing
    assert "targets: ${{ needs.missing.outputs.targets }}" in missing
    assert "version: ${{ needs.missing.outputs.tag }}" in missing
    assert "if: ${{ needs.missing.outputs.targets != '' }}" in missing, "nothing lacking starts nothing"
    build = job_block(missing, "build")
    assert "contents: write" in build
    # Its own concurrency group: release-all.yml's would make it wait for itself.
    assert "group: wena-release-all-missing-" in missing
    # Negative: it builds nothing of its own and never makes a release.
    assert "build_desktop" not in missing and "gh release create" not in missing
    # Which targets: a release's asset names against the ready ones.
    package = module("package_desktop_release")
    ready = [t for t, r in package.targets().items() if r[2] == "ready"]
    every = [package.release_name(t) for t in ready] + ["SHA256SUMS"]
    assert package.missing_from(every) == []
    assert package.missing_from([n for n in every if n not in ("wena-haiku-amd64", "wena-amigaos4-ppc")]) == \
        [t for t in ready if t in ("haiku-amd64", "amigaos4-ppc")]
    assert package.missing_from([]) == ready
    # Negative: a planned target is never asked for.
    assert all(package.targets()[t][2] == "ready" for t in package.missing_from([]))


if __name__ == "__main__":
    main()
    test_missing()
