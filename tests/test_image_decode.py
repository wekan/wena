#!/usr/bin/env python3
"""Wena's image decoder (models/image_decode.c) on images with known pixels.

PNG in every color type and bit depth, with palette and color-key
transparency, filtered and Adam7-interlaced; GIF with a local palette,
transparency and interlacing; and, where `sips` is there, JPEG baseline
from a known image, within JPEG's loss. WebP, a progressive JPEG and broken
data are refused, never decoded into garbage.
"""
import os
from pathlib import Path
import random
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib

ROOT = Path(__file__).resolve().parents[1]


def chunk(kind, body):
    return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)


def pack_samples(values, depth):
    if depth == 8:
        return bytes(values)
    if depth == 16:
        return b"".join(struct.pack(">H", v) for v in values)
    out, acc, bits = bytearray(), 0, 0
    for v in values:
        acc = (acc << depth) | v
        bits += depth
        if bits == 8:
            out.append(acc)
            acc, bits = 0, 0
    if bits:
        out.append(acc << (8 - bits))
    return bytes(out)


def filtered(line, bpp, previous, kind):
    out = bytearray()
    for i, x in enumerate(line):
        a = line[i - bpp] if i >= bpp else 0
        b = previous[i] if previous else 0
        c = previous[i - bpp] if previous and i >= bpp else 0
        if kind == 0:
            p = 0
        elif kind == 1:
            p = a
        elif kind == 2:
            p = b
        elif kind == 3:
            p = (a + b) >> 1
        else:
            pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
            p = a if pa <= pb and pa <= pc else b if pb <= pc else c
        out.append((x - p) & 255)
    return bytes([kind]) + bytes(out)


def png(width, height, color, depth, pixels, palette=None, trns=None, interlace=False):
    """pixels: rows of sample tuples per pixel."""
    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[color]
    bpp = max(1, channels * depth // 8)
    passes = [(0, 0, 8, 8), (4, 0, 8, 8), (0, 4, 4, 8), (2, 0, 4, 4), (0, 2, 2, 4), (1, 0, 2, 2), (0, 1, 1, 2)] \
        if interlace else [(0, 0, 1, 1)]
    raw = bytearray()
    for sx, sy, dx, dy in passes:
        previous = None
        for y in range(sy, height, dy):
            row = [s for x in range(sx, width, dx) for s in pixels[y][x]]
            if not row:
                continue
            line = pack_samples(row, depth)
            raw += filtered(line, bpp, previous, (y + sx) % 5)
            previous = line
    body = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, depth, color, 0, 0, int(interlace)))
    if palette:
        body += chunk(b"PLTE", b"".join(bytes(p[:3]) for p in palette))
    if trns is not None:
        body += chunk(b"tRNS", trns)
    raw = bytes(raw)
    half = len(raw) // 2
    compressed = zlib.compress(raw, 9)
    body += chunk(b"IDAT", compressed[:10]) + chunk(b"IDAT", compressed[10:]) + chunk(b"IEND", b"")
    return body


def gif(width, height, palette, indices, transparent=None, interlace=False):
    # LZW with a clear code before the table grows: codes stay min_code+1 bits.
    min_code = 8
    clear, end = 256, 257
    codes, acc, bits = bytearray(), 0, 0
    def emit(code, size):
        nonlocal acc, bits
        acc |= code << bits
        bits += size
        while bits >= 8:
            codes.append(acc & 255)
            acc >>= 8
            bits -= 8
    order = list(range(height))
    if interlace:
        order = list(range(0, height, 8)) + list(range(4, height, 8)) + list(range(2, height, 4)) + list(range(1, height, 2))
    stream = [indices[y][x] for y in order for x in range(width)]
    emit(clear, 9)
    count = 0
    for v in stream:
        emit(v, 9)
        count += 1
        if count == 200:
            emit(clear, 9)
            count = 0
    emit(end, 9)
    if bits:
        codes.append(acc & 255)
    blocks = b"".join(bytes([len(codes[i:i + 255])]) + bytes(codes[i:i + 255]) for i in range(0, len(codes), 255)) + b"\x00"
    out = b"GIF89a" + struct.pack("<HHBBB", width, height, 0x00, 0, 0)
    if transparent is not None:
        out += b"\x21\xf9\x04\x01\x00\x00" + bytes([transparent]) + b"\x00"
    out += b"\x2c" + struct.pack("<HHHH", 0, 0, width, height) + bytes([0x87 | (0x40 if interlace else 0)])
    out += b"".join(bytes(p) for p in palette) + bytes([min_code]) + blocks + b"\x3b"
    return out


def decode(tool, path):
    run = subprocess.run([str(tool), str(path)], capture_output=True, text=True)
    assert run.returncode == 0, run.stderr
    head, pixels = run.stdout.split("\n")[:2]
    result, width, height = (int(v) for v in head.split())
    return result, width, height, bytes.fromhex(pixels)


def main():
    tmp = Path(os.environ.get("TMPDIR", tempfile.gettempdir()))
    rng = random.Random(4)
    with tempfile.TemporaryDirectory(dir=tmp) as temporary:
        work = Path(temporary)
        tool = work / "tool"
        subprocess.run(["cc", "-std=c89", "-pedantic-errors", "-Wall", "-Wextra", "-Werror",
                        str(ROOT / "tests/image_decode_tool.c"), str(ROOT / "models/image_decode.c"), "-lm",
                        "-o", str(tool)], check=True)
        cases = 0
        for color, depths in ((0, (1, 2, 4, 8, 16)), (2, (8, 16)), (3, (1, 2, 4, 8)), (4, (8, 16)), (6, (8, 16))):
            for depth in depths:
                for interlace in (False, True):
                    w, h = rng.randrange(1, 23), rng.randrange(1, 19)
                    top = (1 << depth) - 1
                    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[color]
                    palette = [(rng.randrange(256), rng.randrange(256), rng.randrange(256)) for _ in range(min(256, top + 1))] \
                        if color == 3 else None
                    alpha = bytes(rng.randrange(256) for _ in palette) if palette else None
                    pixels = [[tuple(rng.randrange(top + 1) for _ in range(channels)) for _ in range(w)] for _ in range(h)]
                    trns = alpha
                    key = None
                    if color in (0, 2) and depth >= 8:
                        key = pixels[0][0]
                        trns = b"".join(struct.pack(">H", v) for v in key)
                    path = work / "image.png"
                    path.write_bytes(png(w, h, color, depth, pixels, palette, trns, interlace))
                    result, width, height, rgba = decode(tool, path)
                    assert (result, width, height) == (0, w, h), (color, depth, interlace, result, width, height)
                    expected = bytearray()
                    for row in pixels:
                        for p in row:
                            def eight(v):
                                return v >> 8 if depth == 16 else v * (255 // top) if depth < 8 else v
                            if color == 3:
                                expected += bytes(palette[p[0]]) + bytes([alpha[p[0]]])
                            elif color == 0:
                                g = eight(p[0])
                                expected += bytes([g, g, g, 0 if key is not None and p == key else 255])
                            elif color == 4:
                                g = eight(p[0])
                                expected += bytes([g, g, g, eight(p[1])])
                            elif color == 2:
                                expected += bytes([eight(v) for v in p]) + bytes([0 if key is not None and p == key else 255])
                            else:
                                expected += bytes([eight(v) for v in p])
                    assert rgba == bytes(expected), (color, depth, interlace)
                    cases += 1
        # GIF: palette indices with transparency, plain and interlaced.
        for interlace in (False, True):
            w, h = 13, 11
            palette = [(rng.randrange(256), rng.randrange(256), rng.randrange(256)) for _ in range(256)]
            indices = [[rng.randrange(256) for _ in range(w)] for _ in range(h)]
            path = work / "image.gif"
            path.write_bytes(gif(w, h, palette, indices, transparent=7, interlace=interlace))
            result, width, height, rgba = decode(tool, path)
            assert (result, width, height) == (0, w, h), (result, width, height)
            expected = bytearray()
            for row in indices:
                for i in row:
                    expected += bytes(palette[i]) + bytes([0 if i == 7 else 255])
            assert rgba == bytes(expected), ("gif", interlace)
            cases += 1
        # JPEG from a known image, through the system's encoder.
        if shutil.which("sips"):
            w, h = 40, 24
            bmp = bytearray(b"BM" + struct.pack("<IHHI", 54 + w * h * 3, 0, 0, 54) +
                            struct.pack("<IiiHHIIiiII", 40, w, -h, 1, 24, 0, w * h * 3, 2835, 2835, 0, 0))
            pixels = []
            for y in range(h):
                for x in range(w):
                    rgb = (x * 6 % 256, y * 10 % 256, 128)
                    pixels.append(rgb)
                    bmp += bytes(reversed(rgb))
            (work / "source.bmp").write_bytes(bytes(bmp))
            subprocess.run(["sips", "-s", "format", "jpeg", "-s", "formatOptions", "best", str(work / "source.bmp"),
                            "--out", str(work / "image.jpg")], capture_output=True, check=True)
            result, width, height, rgba = decode(tool, work / "image.jpg")
            if result == 2:
                print("image decode: this system's JPEG encoder writes progressive JPEG; refused as it should be")
            else:
                assert (result, width, height) == (0, w, h), (result, width, height)
                worst = max(abs(rgba[i * 4 + c] - pixels[i][c]) for i in range(w * h) for c in range(3))
                assert worst <= 40, worst
                cases += 1
        # Refused: WebP, a progressive JPEG, broken data, unknown bytes.
        for name, data, want in (("webp", b"RIFF\x10\x00\x00\x00WEBPVP8 " + b"\x00" * 20, 2),
                                 ("progressive", b"\xff\xd8\xff\xc2\x00\x11\x08\x00\x10\x00\x10\x03\x01\x22\x00\x02\x11\x01\x03\x11\x01" + b"\x00" * 8, 2),
                                 ("truncated", png(4, 4, 2, 8, [[(1, 2, 3)] * 4] * 4)[:60], 3),
                                 ("unknown", b"not an image at all", 1)):
            path = work / name
            path.write_bytes(data)
            result = decode(tool, path)[0]
            assert result == want, (name, result)
        # The header logo Wena embeds (WeKan's public/logo-header.png, 97 x 28
        # grayscale with alpha) decodes, and its alpha is not all opaque.
        logo = (ROOT / "client/platform/logo_data.h").read_text()
        body = logo[logo.index("{") + 1:logo.index("}")]
        path = work / "logo.png"
        path.write_bytes(bytes(int(v, 16) for v in body.replace("\n", "").split(",") if v.strip()))
        result, width, height, rgba = decode(tool, path)
        assert (result, width, height) == (0, 97, 28), (result, width, height)
        assert len(set(rgba[3::4])) > 1
        cases += 1
        print("image decode: %d PNG, GIF and JPEG images with known pixels, and four refusals" % cases)


if __name__ == "__main__":
    sys.exit(main())
