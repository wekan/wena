#!/usr/bin/env python3
"""Embed the pinned trusted TTF; Python standard library only. No font editing."""
import argparse
import hashlib
import json
import pathlib
import struct

ROOT = pathlib.Path(__file__).resolve().parents[1]
ASSET = ROOT / 'third_party/fonts/RobotoStatic-Regular.ttf'
OUTPUT = ROOT / 'client/platform/font_data.h'


def tables(data):
    if len(data) < 12 or len(data) > 1048576 or data[:4] != b'\0\1\0\0':
        raise ValueError('invalid bounded TrueType header')
    count = struct.unpack_from('>H', data, 4)[0]
    if not 1 <= count <= 128 or 12 + count * 16 > len(data):
        raise ValueError('invalid table count')
    result = {}
    spans = []
    for index in range(count):
        tag, checksum, offset, length = struct.unpack_from('>4sIII', data, 12 + index * 16)
        if tag in result or offset % 4 or offset < 12 + count * 16 or offset + length > len(data):
            raise ValueError('invalid table bounds')
        if any(offset < end and start < offset + length for start, end in spans):
            raise ValueError('overlapping tables')
        spans.append((offset, offset + length))
        table = data[offset:offset + length]
        checked = table
        if tag == b'head':
            if length < 54:
                raise ValueError('short head')
            checked = table[:8] + b'\0' * 4 + table[12:]
        checked += b'\0' * (-len(checked) % 4)
        if sum(struct.unpack('>%dI' % (len(checked) // 4), checked)) & 0xffffffff != checksum:
            raise ValueError('table checksum mismatch')
        result[tag] = table
    if not {b'head', b'cmap', b'glyf', b'loca', b'maxp'} <= result.keys():
        raise ValueError('missing TrueType tables')
    return result


def glyphs(data):
    cmap = tables(data)[b'cmap']
    for index in range(struct.unpack_from('>H', cmap, 2)[0]):
        platform, encoding, offset = struct.unpack_from('>HHI', cmap, 4 + index * 8)
        if platform == 3 and encoding == 1 and struct.unpack_from('>H', cmap, offset)[0] == 4:
            length = struct.unpack_from('>H', cmap, offset + 2)[0]
            sub = cmap[offset:offset + length]
            count = struct.unpack_from('>H', sub, 6)[0] // 2
            found = set()
            for index in range(count):
                end = struct.unpack_from('>H', sub, 14 + 2 * index)[0]
                start = struct.unpack_from('>H', sub, 16 + 2 * count + 2 * index)[0]
                delta = struct.unpack_from('>h', sub, 16 + 4 * count + 2 * index)[0]
                pos = 16 + 6 * count + 2 * index
                distance = struct.unpack_from('>H', sub, pos)[0]
                for cp in range(start, end + 1):
                    raw = struct.unpack_from('>H', sub, pos + distance + 2 * (cp - start))[0] if distance else cp
                    if raw and (raw + delta) & 65535:
                        found.add(cp)
            return found
    raise ValueError('no Windows Unicode BMP cmap')


def render(data):
    supported = glyphs(data)
    blocks = ((32, 126), (160, 591), (880, 1327), (7680, 8191), (8192, 8303), (8364, 8364))
    selected = sorted(cp for cp in supported if any(start <= cp <= end for start, end in blocks))
    ranges = []
    for cp in selected:
        if ranges and ranges[-1][1] + 1 == cp:
            ranges[-1][1] = cp
        else:
            ranges.append([cp, cp])
    text = '/* Generated unchanged trusted Roboto bytes; Apache-2.0, see third_party/fonts. */\n'
    text += 'static const unsigned char wena_font_data[] = {\n'
    text += ''.join('    ' + ','.join('0x%02x' % value for value in data[i:i+16]) + ',\n' for i in range(0, len(data), 16))
    text += '};\nstatic const nk_rune wena_font_ranges[] = {\n'
    text += ''.join('    0x%04x, 0x%04x,\n' % tuple(pair) for pair in ranges)
    return text + '    0\n};\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    data = ASSET.read_bytes()
    lock = json.loads((ASSET.parent / 'provenance.json').read_text())
    if len(data) != lock['bytes'] or hashlib.sha256(data).hexdigest() != lock['sha256']:
        raise ValueError('font provenance mismatch')
    license_data = (ASSET.parent / 'LICENSE-Roboto.txt').read_bytes()
    if hashlib.sha256(license_data).hexdigest() != lock['license_sha256']:
        raise ValueError('font license provenance mismatch')
    generated = render(data)
    if args.check:
        if not OUTPUT.exists() or OUTPUT.read_text() != generated:
            raise ValueError('stale native font data; run scripts/generate_native_font.py')
    else:
        OUTPUT.write_text(generated)
    print('native font: pinned bytes, table checksums and generated ranges verified')


if __name__ == '__main__':
    main()
