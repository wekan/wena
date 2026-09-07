#!/usr/bin/env python3
import importlib.util
import json
from pathlib import Path
import shutil
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('dependencies', ROOT / 'scripts/check_dependencies.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class DependenciesTests(unittest.TestCase):
    def test_real_pinned_inputs(self):
        self.assertEqual(module.verify()['nuklear']['header_version'], '4.13.3')

    def test_supported_wal_fix_branches(self):
        for number in [3044006, 3044010, 3050007, 3050010, 3051003, 3053004]:
            self.assertTrue(module.wal_reset_fixed_upstream(number))
        for number in [3044005, 3045001, 3050006, 3051002]:
            self.assertFalse(module.wal_reset_fixed_upstream(number))

    def test_source_archive_provenance_and_tampering(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            names = ['config/dependencies-lock.json'] + list(module.verify()['nuklear']['files'])
            for name in names:
                destination = root / name
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(ROOT / name, destination)
            module.verify(root)
            backend = root / 'third_party/nuklear/demo/sdl_renderer/nuklear_sdl_renderer.h'
            backend.write_bytes(backend.read_bytes() + b'\n/* altered */\n')
            with self.assertRaisesRegex(ValueError, 'checksum'):
                module.verify(root)
            shutil.copyfile(ROOT / backend.relative_to(root), backend)
            (root / 'third_party/nuklear/LICENSE').unlink()
            with self.assertRaisesRegex(ValueError, 'checksum'):
                module.verify(root)


if __name__ == '__main__':
    unittest.main()
