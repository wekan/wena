#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
python3 "$root_dir/tests/test_sqlite_schema.py"
