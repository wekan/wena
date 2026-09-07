#!/usr/bin/env python3
"""Compare actual compiled runtime bytes to all selected canonical values."""
from pathlib import Path
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import generate_ui_i18n as generator


class GeneratorTests(unittest.TestCase):
    def test_literal_octal_and_trigraph_safety(self):
        self.assertEqual(generator.literal('"\\?\nÄ1'), '"\\042\\134\\077\\012\\303\\2041"')

    def test_contract_key_inventory(self):
        keys = generator.ui_keys((ROOT / "imports/ui/page_contract.c").read_text())
        self.assertIn("save", keys)
        self.assertIn("cancel", keys)
        self.assertIn("loginPopup-title", keys)
        self.assertEqual(keys, sorted(set(keys)))
        with self.assertRaises(ValueError):
            generator.ui_keys('no contracts')

    def test_generated_bytes_are_current(self):
        self.assertEqual((ROOT / "imports/i18n/ui_catalog_data.h").read_text(), generator.generate())


def runtime(binary, settings):
    _, keys, languages = generator.selected_catalog()
    completed = subprocess.run([binary, settings], input='\n'.join(keys) + '\n',
                               text=True, capture_output=True, check=True)
    actual = {}
    for line in completed.stdout.splitlines():
        tag, key, value = line.split('\t')
        assert (tag, key) not in actual
        actual[tag, key] = bytes.fromhex(value).decode('utf-8')
    english = dict(zip(keys, next(row for tag, row in languages if tag == 'en')))
    expected = {(tag, key): value or english[key]
                for tag, row in languages for key, value in zip(keys, row)}
    assert actual == expected, "compiled runtime values differ from canonical translations"
    print(f"verified {len(actual)} canonical runtime translations across {len(languages)} languages")


if __name__ == '__main__':
    binary, settings = sys.argv[1:]
    runtime(binary, settings)
    unittest.main(argv=[sys.argv[0]])
