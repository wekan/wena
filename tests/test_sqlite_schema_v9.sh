#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
export WENA_TEST_SETTING_VERSION=9
export WENA_TEST_SETTING_TABLE=board_card_collapse_settings
export WENA_TEST_SETTING_COLUMN=allow_collapse
exec sh "$root_dir/tests/test_sqlite_schema_v7.sh"
