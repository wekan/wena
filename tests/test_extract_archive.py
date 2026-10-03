#!/usr/bin/env python3
"""scripts/extract_archive.py: tar's --strip-components=1, on OpenBSD too.

OpenBSD's tar has no --strip-components, which stopped both OpenBSD builds
in the wena3 release run. The helper unpacks without the top directory,
keeps modes and links, and refuses entries and links that leave it.
"""
import importlib.util
import io
import os
from pathlib import Path
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("extract", ROOT / "scripts" / "extract_archive.py")
extract = importlib.util.module_from_spec(spec)
spec.loader.exec_module(extract)


def archive(path, entries, mode="w:gz"):
    with tarfile.open(path, mode) as bundle:
        for name, kind, target in entries:
            info = tarfile.TarInfo(name)
            data = None
            if kind == "sym":
                info.type, info.linkname = tarfile.SYMTYPE, target
            elif kind == "dir":
                info.type, info.mode = tarfile.DIRTYPE, 0o755
            else:
                data = target.encode()
                info.size, info.mode = len(data), 0o755 if kind == "exec" else 0o644
            bundle.addfile(info, io.BytesIO(data) if data is not None else None)


def main():
    with tempfile.TemporaryDirectory() as temp:
        temp = Path(temp)
        for mode, suffix in (("w:gz", "tar.gz"), ("w:xz", "tar.xz")):
            good = temp / f"good.{suffix}"
            archive(good, [("SDL2-2.32.10/", "dir", ""), ("SDL2-2.32.10/configure", "exec", "#!/bin/sh\n"),
                           ("SDL2-2.32.10/include/SDL.h", "file", "x"),
                           ("SDL2-2.32.10/include/alias.h", "sym", "SDL.h")], mode)
            out = temp / f"out-{suffix}"
            extract.extract(good, out)
            # The top directory is gone; modes and links are kept.
            assert (out / "include" / "SDL.h").read_text() == "x"
            assert os.access(out / "configure", os.X_OK)
            assert os.readlink(out / "include" / "alias.h") == "SDL.h"
            assert not (out / "SDL2-2.32.10").exists()
        # Negative: nothing may land outside the destination.
        for name, entries in (("parent", [("top/../../evil", "file", "x")]),
                              ("absolute-link", [("top/a", "sym", "/etc/passwd")]),
                              ("escaping-link", [("top/a", "sym", "../../outside")])):
            bad = temp / f"{name}.tar.gz"
            archive(bad, entries)
            try:
                extract.extract(bad, temp / f"bad-{name}")
            except ValueError:
                pass
            else:
                raise AssertionError(name)
            assert not (temp / "evil").exists() and not (temp / "outside").exists()
    # The release build uses it, not tar --strip-components.
    script = (ROOT / "scripts" / "build_desktop_release.sh").read_text()
    assert "--strip-components" not in script.replace("# Not tar --strip-components", "")
    assert script.count("scripts/extract_archive.py") == 2
    print("extract-archive: gzip and xz unpacked without the top directory; escapes refused")


if __name__ == "__main__":
    main()
