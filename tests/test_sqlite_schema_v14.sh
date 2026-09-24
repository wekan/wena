#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
export WENA_TEST_SETTING_VERSION=14
export WENA_TEST_SETTING_TABLE=board_members
export WENA_TEST_SETTING_COLUMN=active
exec sh "$root_dir/tests/test_sqlite_schema_v7.sh"
