#!/usr/bin/env python3
"""Check that a release desktop executable is self-contained.

    check_release_executable.py TARGET EXECUTABLE

Reads the executable's own dynamic-library list from its headers (no tool
needed, so it works for cross-compiled files): ELF DT_NEEDED, Mach-O
LC_LOAD_DYLIB, or the PE import table. Fails when SDL2 or SQLite is loaded at
run time, when a Windows build needs a MinGW runtime DLL, or when the file is
not the format and CPU the target names. Prints the libraries it does need.
"""

import struct
import sys

ELF_MACHINES = {"amd64": 62, "arm64": 183, "armhf": 40, "armel": 40, "i686": 3, "riscv64": 243,
                "ppc64le": 21, "s390x": 22, "loong64": 258, "mips64le": 8}
PE_MACHINES = {"amd64": 0x8664, "i686": 0x14C, "arm64": 0xAA64}
MACHO_CPUS = {"amd64": 0x01000007, "arm64": 0x0100000C}
# System DLLs a Windows build may import; anything else must be linked in.
WINDOWS_SYSTEM = {"kernel32.dll", "user32.dll", "gdi32.dll", "winmm.dll", "imm32.dll", "ole32.dll",
                  "oleaut32.dll", "version.dll", "setupapi.dll", "shell32.dll", "advapi32.dll",
                  "msvcrt.dll", "ucrtbase.dll", "cfgmgr32.dll", "dinput8.dll", "shlwapi.dll",
                  "comdlg32.dll", "ws2_32.dll", "bcrypt.dll"}
FORBIDDEN = ("sdl2", "sqlite")
# Systems whose executables are ELF, checked alike: the CPU, and that SDL2 and
# SQLite are linked in. Their C library and windowing system are the system's.
ELF_SYSTEMS = ("linux", "freebsd", "netbsd", "openbsd", "dragonflybsd", "haiku")


def cstring(data, offset):
    end = data.index(b"\0", offset)
    return data[offset:end].decode("utf-8", "replace")


def elf_needed(data):
    if data[:4] != b"\x7fELF":
        raise ValueError("not an ELF executable")
    bits64, little = data[4] == 2, data[5] == 1
    e = "<" if little else ">"
    machine = struct.unpack_from(e + "H", data, 18)[0]
    if bits64:
        phoff, = struct.unpack_from(e + "Q", data, 32); phentsize, phnum = struct.unpack_from(e + "HH", data, 54)
    else:
        phoff, = struct.unpack_from(e + "I", data, 28); phentsize, phnum = struct.unpack_from(e + "HH", data, 42)
    loads, dynamic = [], None
    for i in range(phnum):
        base = phoff + i * phentsize
        if bits64:
            ptype, _flags, offset, vaddr, _paddr, filesz = struct.unpack_from(e + "IIQQQQ", data, base)
        else:
            ptype, offset, vaddr, _paddr, filesz = struct.unpack_from(e + "IIIII", data, base)
        if ptype == 1:
            loads.append((vaddr, offset, filesz))
        elif ptype == 2:
            dynamic = (offset, filesz)
    if dynamic is None:
        return machine, []          # fully static
    entries, strtab, needed = [], None, []
    size = 16 if bits64 else 8
    for pos in range(dynamic[0], dynamic[0] + dynamic[1], size):
        tag, value = struct.unpack_from(e + ("qQ" if bits64 else "iI"), data, pos)
        if tag == 0:
            break
        if tag == 5:
            strtab = value
        elif tag == 1:
            entries.append(value)
    for vaddr, offset, filesz in loads:
        if strtab is not None and vaddr <= strtab < vaddr + filesz:
            strtab = strtab - vaddr + offset
            break
    return machine, [cstring(data, strtab + value) for value in entries]


def macho_needed(data, offset=0):
    magic, = struct.unpack_from("<I", data, offset)
    if magic != 0xFEEDFACF:
        raise ValueError("not a 64-bit Mach-O executable")
    cpu, _sub, _type, ncmds, _size = struct.unpack_from("<iiIII", data, offset + 4)
    pos, needed = offset + 32, []
    for _ in range(ncmds):
        cmd, cmdsize = struct.unpack_from("<II", data, pos)
        if cmd in (0xC, 0x80000018, 0x8000001F):     # LOAD_DYLIB, LOAD_WEAK_DYLIB, REEXPORT
            name_offset, = struct.unpack_from("<I", data, pos + 8)
            needed.append(cstring(data, pos + name_offset))
        pos += cmdsize
    return cpu & 0xFFFFFFFF, needed


def pe_needed(data):
    if data[:2] != b"MZ":
        raise ValueError("not a PE executable")
    pe, = struct.unpack_from("<I", data, 0x3C)
    if data[pe:pe + 4] != b"PE\0\0":
        raise ValueError("not a PE executable")
    machine, sections, _ts, _sym, _nsym, optsize, _chars = struct.unpack_from("<HHIIIHH", data, pe + 4)
    opt = pe + 24
    magic, = struct.unpack_from("<H", data, opt)
    dirs = opt + (112 if magic == 0x20B else 96)
    import_rva, _import_size = struct.unpack_from("<II", data, dirs + 8)
    table = opt + optsize
    sec = [struct.unpack_from("<8sIIII", data, table + 40 * i) for i in range(sections)]

    def offset(rva):
        for _name, vsize, vaddr, rawsize, rawptr in sec:
            if vaddr <= rva < vaddr + max(vsize, rawsize):
                return rva - vaddr + rawptr
        raise ValueError("RVA outside sections")
    needed, pos = [], offset(import_rva) if import_rva else None
    while pos is not None:
        name_rva, = struct.unpack_from("<I", data, pos + 12)
        if name_rva == 0:
            break
        needed.append(cstring(data, offset(name_rva)))
        pos += 20
    return machine, needed


def check(target, data):
    system, cpu = target.split("-", 1)
    if system in ELF_SYSTEMS:
        machine, needed = elf_needed(data)
        if machine != ELF_MACHINES.get(cpu):
            raise ValueError(f"ELF machine {machine} is not {cpu}")
    elif system == "macos":
        if data[:4] == b"\xca\xfe\xba\xbe":
            raise ValueError("universal Mach-O: check each architecture separately")
        machine, needed = macho_needed(data)
        if machine != MACHO_CPUS.get(cpu):
            raise ValueError(f"Mach-O CPU {machine:#x} is not {cpu}")
        outside = [n for n in needed if not n.startswith(("/usr/lib/", "/System/Library/"))]
        if outside:
            raise ValueError("loads libraries outside the system: " + ", ".join(outside))
    elif system == "windows":
        machine, needed = pe_needed(data)
        if machine != PE_MACHINES.get(cpu):
            raise ValueError(f"PE machine {machine:#x} is not {cpu}")
        extra = [n for n in needed if n.lower() not in WINDOWS_SYSTEM and not n.lower().startswith("api-ms-win-")]
        if extra:
            raise ValueError("needs DLLs that are not part of Windows: " + ", ".join(extra))
    else:
        raise ValueError(f"unknown target {target}")
    linked = [n for n in needed if any(word in n.lower() for word in FORBIDDEN)]
    if linked:
        raise ValueError("loads at run time what must be linked in: " + ", ".join(linked))
    return needed


def main(argv):
    if len(argv) != 2:
        print("Usage: check_release_executable.py TARGET EXECUTABLE", file=sys.stderr)
        return 2
    try:
        with open(argv[1], "rb") as stream:
            needed = check(argv[0], stream.read())
    except (OSError, ValueError, struct.error, IndexError) as error:
        print(f"check_release_executable: {argv[1]}: {error}", file=sys.stderr)
        return 1
    print(f"{argv[0]}: self-contained; system libraries: {', '.join(needed) or 'none'}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
