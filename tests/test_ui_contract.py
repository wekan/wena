#!/usr/bin/env python3
"""Check the shared UI contract against canonical WeKan translations."""

import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import zlib


ROOT = Path(__file__).resolve().parents[1]


def english_keys():
    raw = (ROOT / "imports" / "i18n" / "wekan-i18n.bin").read_bytes()
    payload = json.loads(zlib.decompress(raw[20:]).decode("utf-8"))
    english = next(language for language in payload["languages"] if language["tag"] == "en")
    return {key for key, _value in english["entries"]}


def main():
    source = (ROOT / "imports" / "ui" / "page_contract.c").read_text(encoding="utf-8")
    keys = english_keys()
    contract_keys = re.findall(r'\{(?:WENA_UI_[A-Z_]+|"/[^"]*"), "([^"]+)"', source)
    assert contract_keys
    assert not sorted(set(contract_keys) - keys), "contract contains noncanonical i18n keys"
    expected_routes = [line for line in
                       (ROOT / "tests" / "fixtures" /
                        "meteor_legacy_html4_routes.txt").read_text(encoding="utf-8").splitlines()
                       if line and not line.startswith("#")]
    routes = re.findall(r'\{"(/[^"]*)", "[^"]+"\}', source)
    assert routes == expected_routes
    with tempfile.TemporaryDirectory() as temporary:
        binary = Path(temporary) / "ui-contract-test"
        subprocess.run([
            "cc", "-std=c89", "-pedantic-errors", "-Wall", "-Wextra", "-Werror",
            str(ROOT / "tests" / "ui_contract_test.c"),
            str(ROOT / "imports" / "ui" / "page_contract.c"), "-o", str(binary),
        ], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
