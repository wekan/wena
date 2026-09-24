#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
import hashlib
import json
from pathlib import Path
import re
import struct
import sys
import unittest
import xml.etree.ElementTree as ET
import zlib
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
import compile_svg
import png_to_svg


class SvgTests(unittest.TestCase):
    def test_generated_vectors_and_tokens_current(self):
        for name,output in compile_svg.generate().items():
            self.assertEqual((ROOT/name).read_text(),output)
        self.assertLess((ROOT/'client/platform/svg_data.h').stat().st_size,1024)

    def test_subset_rejects_unsupported_inputs(self):
        valid = (ROOT/'imports/ui/svg/board.svg').read_bytes()
        for wrong in [valid.replace(b'<rect',b'<image',1),valid.replace(b'<rect',b'<script',1),
                      valid.replace(b'x="2"',b'x="NaN"'),valid.replace(b'width="20"',b'width="-1"'),
                      valid.replace(b'rx="2"',b'rx="12"'),valid.replace(b'currentColor',b'url(https://example.org)'),
                      valid.replace(b'<rect',b'<rect transform="scale(2)"',1),
                      valid.replace(b'viewBox="0 0 24 24"',b'viewBox="0 0 0 24"'),
                      valid.replace(b'<rect',b'<rect onclick="run()"',1),
                      b'<!DOCTYPE svg>'+valid,valid+b' '*65536]:
            with self.subTest(value=wrong[:100]),self.assertRaises((ValueError,ET.ParseError)):
                compile_svg.parse(wrong)
        _,shapes=compile_svg.parse(b'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">'
            b'<circle cx="12" cy="12" r="4" fill="#123456"/>'
            b'<line x1="2" y1="3" x2="22" y2="20" stroke="currentColor"/></svg>')
        self.assertEqual([s[0] for s in shapes],['circle','line'])

    def test_historical_capture_pixels_and_provenance(self):
        records=json.loads((ROOT/'docs/images/provenance.json').read_text())
        for name,record in records.items():
            with self.subTest(name=name):
                root=ET.parse(ROOT/'docs/images'/name).getroot()
                width,height=record['width'],record['height']
                pixels=bytearray(width*height*4)
                covered=bytearray(width*height)
                for element in root:
                    if element.tag.endswith('title'):continue
                    self.assertEqual(element.tag,compile_svg.SVG+'path')
                    rgb=bytes.fromhex(element.attrib['fill'][1:])
                    rgba=rgb+bytes([round(float(element.get('fill-opacity','1'))*255)])
                    path=element.attrib['d']
                    pattern=r'M(\d+) (\d+)h(\d+)v(\d+)h-(\d+)z'
                    matches=list(re.finditer(pattern,path))
                    self.assertEqual(''.join(m[0] for m in matches),path)
                    for match in matches:
                        x,y,w,h,back=map(int,match.groups());self.assertEqual(w,back)
                        self.assertTrue(w>0 and h>0 and x+w<=width and y+h<=height)
                        for row in range(y,y+h):
                            start=row*width+x
                            self.assertFalse(any(covered[start:start+w]))
                            covered[start:start+w]=b'\1'*w
                            pixels[start*4:(start+w)*4]=rgba*w
                self.assertTrue(all(covered))
                self.assertEqual(hashlib.sha256(pixels).hexdigest(),record['rgba_sha256'])
        self.assertFalse(list((ROOT/'docs/images').glob('*.png')))

    def test_png_decoder_filters_crc_and_limits(self):
        def chunk(kind,payload):
            return struct.pack('>I',len(payload))+kind+payload+struct.pack('>I',zlib.crc32(kind+payload))
        # Two pixels, RGBA, with each standard PNG row filter independently encoded.
        pixels=bytes([10,20,30,255,40,50,60,128])
        for mode in range(5):
            filtered=bytearray()
            for i,b in enumerate(pixels):
                left=pixels[i-4] if i>=4 else 0
                predictor=left if mode in (1,4) else left//2 if mode==3 else 0
                filtered.append((b-predictor)&255)
            raw=bytes([mode])+filtered
            data=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',2,1,8,6,0,0,0))
            data+=chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b'')
            _,_,rows=png_to_svg.decode(data)
            self.assertEqual(bytes(c for pixel in rows[0] for c in pixel),pixels)
            svg,_=png_to_svg.vectorize(data)
            self.assertNotIn('<image',svg);self.assertNotIn('base64',svg)
            with self.assertRaises(ValueError):png_to_svg.decode(data[:-1])
            with self.assertRaises(ValueError):png_to_svg.decode(data[:-5]+b'xxxxx')

    def test_no_raster_runtime_assets(self):
        for directory in ['client','imports/ui']:
            for p in (ROOT/directory).rglob('*'):
                self.assertNotIn(p.suffix.lower(),{'.png','.jpg','.jpeg','.gif','.bmp','.webp','.ico'})
        self.assertIn('"$root_dir/client/platform/svg.c"',(ROOT/'scripts/build_desktop.sh').read_text())
        for p in ['scripts/compile_svg.py','scripts/png_to_svg.py','client/platform/svg.c','client/platform/svg.h']:
            self.assertIn('SPDX-License-Identifier: MIT',(ROOT/p).read_text())

if __name__=='__main__':unittest.main()
