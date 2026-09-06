#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir="${TMPDIR:-/tmp}/wena-runtime-test-$$"
mkdir -p "$test_dir"
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
"$root_dir/build.sh" build host
target=$(python3 -c "import sys;sys.path.insert(0,'$root_dir/scripts');import wena;print(wena.host_target())")
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
  "$root_dir/tests/runtime_test.c" "$root_dir/server/runtime.c" \
  "$root_dir/server/embedded_migration.c" "$root_dir/server/sqlite_persistence.c" \
  "$root_dir/server/sqlite_storage.c" "$root_dir/server/sha256.c" \
  "$root_dir/server/http_listener.c" "$root_dir/server/http.c" \
  "$root_dir/server/capability.c" "$root_dir/server/settings.c" \
  "$root_dir/server/response_policy.c" "$root_dir/server/router.c" \
  "$root_dir/server/security.c" "$root_dir/server/domain_operation.c" \
  "$root_dir/server/region_response.c" "$root_dir/server/legacy_html4.c" \
  "$root_dir/server/root_url.c" "$root_dir/imports/ui/page_contract.c" \
  "$root_dir/client/features/server_settings.c" \
  "$root_dir/client/features/server_runtime_settings.c" \
  -lsqlite3 -o "$test_dir/test"
"$test_dir/test" "$root_dir/server/migrations/001_initial.sql" "$test_dir" \
  "$root_dir/dist/$target/wena"
