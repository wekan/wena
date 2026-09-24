#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
export WENA_TEST_SETTING_VERSION=11
export WENA_TEST_SETTING_TABLE=list_colors
export WENA_TEST_SETTING_COLUMN=color
exec sh "$root_dir/tests/test_sqlite_schema_v7.sh"
