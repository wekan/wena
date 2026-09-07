#!/usr/bin/env python3
import importlib.util
import pathlib
import unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('font', ROOT / 'scripts/generate_native_font.py')
font = importlib.util.module_from_spec(spec)
spec.loader.exec_module(font)
class FontTests(unittest.TestCase):
    def test_canonical_glyphs(self):
        glyphs = font.glyphs(font.ASSET.read_bytes())
        self.assertTrue(set(map(ord, 'ÄäÖöÅåΩαЖяԯ€')) <= glyphs)
    def test_malformed(self):
        data = font.ASSET.read_bytes()
        for corrupt in (b'', data[:11], data[:-1], b'OTTO' + data[4:], data[:4] + b'\xff\xff' + data[6:]):
            with self.assertRaises(ValueError):
                font.tables(corrupt)
        corrupt = bytearray(data)
        corrupt[-8] ^= 1
        with self.assertRaises(ValueError):
            font.tables(bytes(corrupt))
    def test_overlap(self):
        data = bytearray(font.ASSET.read_bytes())
        data[36:40] = data[20:24]
        with self.assertRaises(ValueError):
            font.tables(bytes(data))
if __name__ == '__main__':
    unittest.main()
