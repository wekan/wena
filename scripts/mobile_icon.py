#!/usr/bin/env python3
"""Wena's app icon for Android and iOS, drawn here as a PNG.

    mobile_icon.py SIZE OUTPUT_PNG

A board in WeKan blue (the desktop's background, 41,128,185) with three white
lists of cards. Drawn from code, byte for byte the same on every build host,
so no image tool and no binary file in the repository are needed. Opaque and
square: iOS rounds the corners itself and refuses an icon with transparency.
"""

from pathlib import Path
import struct
import sys
import zlib

BLUE = (41, 128, 185)
WHITE = (255, 255, 255)
CARD = (214, 234, 248)


def pixels(size):
    """Rows of RGB tuples."""
    image = [[BLUE] * size for _ in range(size)]

    def fill(left, top, right, bottom, color):
        for y in range(max(0, int(top * size)), min(size, int(bottom * size))):
            row = image[y]
            for x in range(max(0, int(left * size)), min(size, int(right * size))):
                row[x] = color

    # Three lists of decreasing length, each with a few cards.
    for index, cards in enumerate((3, 2, 1)):
        left = 0.16 + index * 0.24
        fill(left, 0.18, left + 0.20, 0.30 + cards * 0.17, WHITE)
        for card in range(cards):
            top = 0.24 + card * 0.17
            fill(left + 0.03, top, left + 0.17, top + 0.12, CARD)
    return image


def png(size):
    raw = b"".join(b"\x00" + bytes(channel for pixel in row for channel in pixel)
                   for row in pixels(size))

    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    return (b"\x89PNG\r\n\x1a\n" +
            chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 2, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(raw, 9)) +
            chunk(b"IEND", b""))


def main(argv):
    if len(argv) != 2 or not argv[0].isdigit() or not 16 <= int(argv[0]) <= 1024:
        print("Usage: mobile_icon.py SIZE(16-1024) OUTPUT_PNG", file=sys.stderr)
        return 2
    output = Path(argv[1])
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(png(int(argv[0])))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
