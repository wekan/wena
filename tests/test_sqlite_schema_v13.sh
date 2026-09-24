#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
export WENA_TEST_SETTING_VERSION=13
export WENA_TEST_SETTING_TABLE=swimlane_archive_state
export WENA_TEST_SETTING_COLUMN=archived
exec sh "$root_dir/tests/test_sqlite_schema_v7.sh"
