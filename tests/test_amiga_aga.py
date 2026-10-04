#!/usr/bin/env python3
"""The AmigaOS 3 AGA build: SDL's patch applies to the pinned fork and never
lets the c2p write outside a screen laid out for it; the desktop's AGA branch
draws a 640x512 frame through the palette and still builds; the target is
in the catalog, the workflow and the release check."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PATCH = ROOT / "scripts" / "patches" / "sdl2-amigaos3-aga.patch"
sys.path.insert(0, str(ROOT / "scripts"))


def test_patch():
    text = PATCH.read_text(encoding="utf-8")
    # Only the two files, no timestamps (reproducible), and the guard.
    assert re.findall(r"^\+\+\+ b/(\S+)$", text, re.M) == ["CMakeLists.txt", "src/video/amigaos3/SDL_os3aga.c"]
    assert not re.search(r"^(---|\+\+\+) \S+[ \t]", text, re.M)
    assert "SDL_AMIGAOS3_C2P_BPLSIZE_VALUE=${SDL_AMIGAOS3_C2P_BPLSIZE}" in text
    added = "\n".join(line[1:] for line in text.splitlines() if line.startswith("+"))
    assert "OS3_AGA_PlanesContiguous" in added and "WriteChunkyPixels(" in added
    # The c2p call sits behind the layout check; WriteChunkyPixels is the
    # path for everything else.
    guard = added.index("if (OS3_AGA_PlanesContiguous(bm, width, height))")
    assert guard < added.index("c2p1x1_8_c5_gen((void *)chunky") < added.index("WriteChunkyPixels(")
    for check in ("bm->Depth != 8", "(width & 31) != 0", "bm->BytesPerRow * 8 != width", "bm->Rows != height",
                  "Planes[i] != (const UBYTE *)bm->Planes[0] + (long)i *"):
        assert check in added, check


def test_patch_applies_to_the_pinned_fork():
    from fetch_release_dependency import fetch
    cache = ROOT / ".tools" / "cache"
    try:
        archive = fetch("sdl2-amigaos3", cache)
    except Exception as error:  # offline: the text checks above still ran
        print("skipping the apply check, no source archive:", error)
        return
    patch = shutil.which("patch")
    assert patch, "patch(1) is needed"
    with tempfile.TemporaryDirectory(dir=os.environ.get("TMPDIR")) as temp:
        with tarfile.open(archive) as bundle:
            for member in bundle.getmembers():
                parts = member.name.split("/", 1)
                if len(parts) == 2 and parts[1] in ("CMakeLists.txt", "src/video/amigaos3/SDL_os3aga.c"):
                    member.name = parts[1]
                    bundle.extract(member, temp)
        result = subprocess.run([patch, "-p1", "-s", "--dry-run"], cwd=temp, stdin=PATCH.open("rb"),
                                capture_output=True)
        assert result.returncode == 0, result.stdout + result.stderr
        # Negative: applied twice is refused, not doubled.
        subprocess.run([patch, "-p1", "-s"], cwd=temp, stdin=PATCH.open("rb"), check=True)
        again = subprocess.run([patch, "-p1", "-s", "--dry-run", "--forward"], cwd=temp,
                               stdin=PATCH.open("rb"), capture_output=True)
        assert again.returncode != 0


def test_desktop_branch():
    desktop = (ROOT / "client" / "desktop.c").read_text(encoding="utf-8")
    branch = desktop[desktop.index("#if defined(WENA_AMIGA_AGA)\n/* The AmigaOS 3 AGA build."):]
    branch = branch[:branch.index("#define DESKTOP_RESIZABLE SDL_WINDOW_RESIZABLE")]
    assert "#define DESKTOP_WIDTH 640" in branch and "#define DESKTOP_HEIGHT 512" in branch
    assert "#define DESKTOP_RESIZABLE 0" in branch
    assert "wena_aga_palette_convert(" in branch and "SDL_SetPaletteColors(" in branch
    assert "SDL_BlitSurface(aga_frame, NULL, screen, NULL)" in branch, "an RTG screen gets the frame as it is"
    assert "SDL_WaitEventTimeout(NULL, 1000)" in branch
    # WeKan's UI colors and its label and board colors go in exact.
    assert "wena_wekan_rgb((WenaWekanColor)index)" in branch and "wena_color_contracts(" in branch
    assert "SDL_CreateSoftwareRenderer(aga_frame)" in desktop
    assert desktop.count("if (!desktop_aga_present(window)) DESKTOP_FAIL();") == 2, "both pages present"
    assert "header_icons_collapsed = DESKTOP_AGA;" in desktop
    # Negative: the other builds keep their 1024x720 resizable window.
    assert "#define DESKTOP_WIDTH 1024" in desktop and "#define DESKTOP_HEIGHT 720" in desktop


def test_catalog_workflow_and_check():
    catalog = (ROOT / "config" / "targets.tsv").read_text(encoding="utf-8")
    assert "amigaos-m68k-aga\tAmigaOS 3.x m68k (68040, AGA)\tamiga\tready\twena-amigaos-m68k-aga\n" in catalog
    workflow = (ROOT / ".github" / "workflows" / "release-all.yml").read_text(encoding="utf-8")
    assert "- { target: amigaos-m68k-aga }" in workflow
    import check_release_executable as check
    hunk = b"\x00\x00\x03\xf3" + b"\0" * 60
    assert check.check("amigaos-m68k-aga", hunk) == []
    # Negative: anything but a HUNK executable is refused.
    try:
        check.check("amigaos-m68k-aga", b"\x7fELF" + b"\0" * 60)
    except ValueError as error:
        assert "HUNK" in str(error)
    else:
        raise AssertionError("an ELF file passed as an AmigaOS 3 AGA executable")


def test_desktop_builds_with_aga():
    """The AGA branch compiles strict C89 and runs a smoke frame here (SDL's
    window surface on this machine is 32-bit, so this is the RTG copy; the
    8-bit path is the palette module's own test)."""
    if shutil.which("sdl2-config") is None:
        print("skipping the AGA host build: no sdl2-config")
        return
    with tempfile.TemporaryDirectory(dir=os.environ.get("TMPDIR")) as temp:
        executable = Path(temp) / "wena-aga"
        env = dict(os.environ, WENA_CFLAGS="-DWENA_AMIGA_AGA=1")
        subprocess.run(["sh", str(ROOT / "scripts" / "build_desktop.sh"), str(executable)], env=env,
                       check=True, capture_output=True)
        shot = Path(temp) / "aga.bmp"
        run_env = dict(os.environ, WRITABLE_PATH=str(Path(temp) / "root"), WENA_LOG_DIR=str(Path(temp) / "log"),
                       SDL_VIDEODRIVER=os.environ.get("WENA_TEST_VIDEODRIVER", os.environ.get("SDL_VIDEODRIVER", "")))
        if not run_env["SDL_VIDEODRIVER"]:
            run_env.pop("SDL_VIDEODRIVER")
        run = subprocess.run([str(executable), "--screenshot", str(shot)], env=run_env, capture_output=True,
                             text=True, timeout=60)
        assert run.returncode == 0, run.stdout + run.stderr
        data = shot.read_bytes()
        # An 8-bit BMP of 640x512: what the AGA screen is given.
        assert data[:2] == b"BM" and int.from_bytes(data[18:22], "little") == 640
        assert abs(int.from_bytes(data[22:26], "little", signed=True)) == 512
        assert int.from_bytes(data[28:30], "little") == 8
        log = (Path(temp) / "log" / "desktop.log").read_text()
        assert "AGA palette: " in log and "AGA palette)" in log, log


if __name__ == "__main__":
    for name, function in sorted(globals().items()):
        if name.startswith("test_") and callable(function):
            function()
    print("AmigaOS 3 AGA build: patch, desktop branch, catalog, workflow and release check passed")
