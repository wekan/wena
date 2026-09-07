#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/wena-language-storage-XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
 -Dfflush=wena_test_fflush -Drename=wena_test_rename \
 -c "$root_dir/imports/i18n/language.c" -o "$test_dir/language.o"
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror \
 "$root_dir/tests/language_storage_test.c" "$test_dir/language.o" \
 "$root_dir/imports/i18n/locale.c" -o "$test_dir/test"
"$test_dir/test" "$test_dir"
