#!/usr/bin/env python3
"""Unpack a pinned .tar.gz or .tar.xz without its top directory.

    extract_archive.py ARCHIVE DESTINATION

What `tar -xf ARCHIVE -C DESTINATION --strip-components=1` does, on every
system a release builds on: OpenBSD's tar has no --strip-components, and
Python's tarfile reads gzip and xz the same everywhere. Modes and symbolic
links are kept (llvm-mingw needs both); an entry that would land outside
DESTINATION is refused and nothing more is unpacked.
"""
from pathlib import Path, PurePosixPath
import sys
import tarfile


def extract(archive, destination):
    destination = Path(destination).resolve()
    destination.mkdir(parents=True, exist_ok=True)
    with tarfile.open(archive) as bundle:
        members = []
        for member in bundle.getmembers():
            parts = PurePosixPath(member.name).parts[1:]
            if not parts:
                continue
            if member.name.startswith("/") or ".." in parts:
                raise ValueError(f"{archive}: entry outside its folder: {member.name}")
            if member.issym() and (member.linkname.startswith("/") or
                                   ".." in PurePosixPath(member.linkname).parts[:-1] and
                                   not (destination / PurePosixPath(*parts)).parent.joinpath(member.linkname)
                                   .resolve().is_relative_to(destination)):
                raise ValueError(f"{archive}: link outside its folder: {member.name}")
            if member.islnk():
                link = PurePosixPath(member.linkname).parts[1:]
                if not link or ".." in link:
                    raise ValueError(f"{archive}: hard link outside its folder: {member.name}")
                member.linkname = str(PurePosixPath(*link))
            member.name = str(PurePosixPath(*parts))
            members.append(member)
        # Python 3.12 and later want an extraction filter; "tar" keeps modes and links.
        options = {"filter": "tar"} if hasattr(tarfile, "tar_filter") else {}
        bundle.extractall(destination, members, **options)


def main(argv):
    if len(argv) != 2:
        print(__doc__, file=sys.stderr)
        return 2
    try:
        extract(argv[0], argv[1])
    except (OSError, ValueError, tarfile.TarError) as error:
        print(f"extract_archive: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
