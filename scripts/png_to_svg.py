#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Losslessly convert bounded RGB/RGBA PNG captures to standalone SVG paths.

Offline documentation tool, never linked into Wena. Supports 8-bit noninterlaced
RGB/RGBA only, preserving pixel colors (including antialiasing) without embedding
a raster image or substituting text/fonts. Adjacent equal spans become rectangles.
"""
import argparse
from collections import defaultdict
import hashlib
import json
from pathlib import Path
import struct
import zlib


def decode(data):
    if len(data) > 16*1024*1024 or not data.startswith(b'\x89PNG\r\n\x1a\n'):
        raise ValueError('invalid or oversized PNG')
    offset = 8
    header = None
    compressed = bytearray()
    ended = False
    while offset < len(data):
        if offset+12 > len(data):
            raise ValueError('truncated PNG chunk')
        size = struct.unpack_from('>I', data, offset)[0]
        kind = data[offset+4:offset+8]
        end = offset+8+size
        if end+4 > len(data) or zlib.crc32(data[offset+4:end]) != struct.unpack_from('>I',data,end)[0]:
            raise ValueError('invalid PNG chunk or CRC')
        payload = data[offset+8:end]
        if header is None and kind != b'IHDR':
            raise ValueError('missing PNG header')
        if kind == b'IHDR':
            if header is not None or size != 13:
                raise ValueError('invalid duplicate PNG header')
            header = struct.unpack('>IIBBBBB', payload)
        elif kind == b'IDAT':
            compressed.extend(payload)
        elif kind == b'IEND':
            if size or end+4 != len(data):
                raise ValueError('invalid PNG end')
            ended = True
        else:
            raise ValueError('unsupported PNG chunk (including profiles or transparency)')
        offset = end+4
    if not ended or header is None:
        raise ValueError('incomplete PNG')
    width,height,depth,color,compression,filtering,interlace = header
    if not (0 < width <= 4096 and 0 < height <= 4096 and width*height <= 4*1024*1024) or \
            depth != 8 or color not in (2,6) or compression or filtering or interlace:
        raise ValueError('expected bounded 8-bit RGB/RGBA noninterlaced PNG')
    channels = 4 if color == 6 else 3
    stride = width*channels
    expected = (stride+1)*height
    stream = zlib.decompressobj()
    raw = stream.decompress(compressed, expected+1)
    if len(raw) != expected or not stream.eof or stream.unused_data:
        raise ValueError('invalid PNG decompressed length')
    rows = []
    previous = bytearray(stride)
    for y in range(height):
        start = y*(stride+1)
        mode = raw[start]
        if mode > 4:
            raise ValueError('invalid PNG filter')
        row = bytearray(raw[start+1:start+1+stride])
        for i in range(stride):
            left = row[i-channels] if i >= channels else 0
            up = previous[i]
            upper_left = previous[i-channels] if i >= channels else 0
            if mode == 1: predictor = left
            elif mode == 2: predictor = up
            elif mode == 3: predictor = (left+up)//2
            elif mode == 4:
                p = left+up-upper_left
                distances = (abs(p-left),abs(p-up),abs(p-upper_left))
                predictor = (left,up,upper_left)[distances.index(min(distances))]
            else: predictor = 0
            row[i] = (row[i]+predictor)&255
        rows.append([tuple(row[i:i+channels]) + (() if channels == 4 else (255,))
                     for i in range(0,stride,channels)])
        previous = row
    return width,height,rows


def vectorize(data):
    width,height,rows = decode(data)
    # Disjoint rectangles also preserve alpha; no overlap/compositing changes.
    groups = defaultdict(list)
    active = {}
    for y,row in enumerate(rows):
        current = {}
        x = 0
        while x < width:
            end = x+1
            while end < width and row[end] == row[x]: end += 1
            key = x,end-x,row[x]
            top = active.pop(key, y)
            current[key] = top
            x = end
        for (x,w,color),top in active.items():
            groups[color].append((x,top,w,y-top))
        active = current
    for (x,w,color),top in active.items():
        groups[color].append((x,top,w,height-top))
    output = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d" shape-rendering="crispEdges">\n' % (width,height)
    output += '<!-- SPDX-License-Identifier: MIT. Lossless vector conversion of a historical screenshot. -->\n'
    output += '<title>Historical Wena desktop screenshot</title>\n'
    for (r,g,b,a),rectangles in sorted(groups.items()):
        opacity = '' if a == 255 else ' fill-opacity="%.9f"' % (a/255)
        paths = ''.join('M%d %dh%dv%dh-%dz' % (x,y,w,h,w) for x,y,w,h in rectangles)
        output += '<path fill="#%02x%02x%02x"%s d="%s"/>\n' % (r,g,b,opacity,paths)
    output += '</svg>\n'
    pixels = bytes(channel for row in rows for pixel in row for channel in pixel)
    return output, {'width':width,'height':height,'png_sha256':hashlib.sha256(data).hexdigest(),
                    'rgba_sha256':hashlib.sha256(pixels).hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('destination', type=Path)
    args = parser.parse_args()
    if args.destination.exists():
        raise SystemExit('destination already exists')
    svg, record = vectorize(args.source.read_bytes())
    args.destination.write_text(svg)
    print(json.dumps(record, indent=2))

if __name__ == '__main__':
    main()
