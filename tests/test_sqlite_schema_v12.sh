#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
export WENA_TEST_SETTING_VERSION=12
export WENA_TEST_SETTING_TABLE=list_wip_limits
export WENA_TEST_SETTING_COLUMN=enabled
exec sh "$root_dir/tests/test_sqlite_schema_v7.sh"
