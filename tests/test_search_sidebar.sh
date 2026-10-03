#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/wena-search-sidebar-XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c89 -pedantic-errors -Wall -Wextra -Werror -I"$root_dir/tests/fakes" \
  "$root_dir/tests/search_sidebar_test.c" "$root_dir/client/components/sidebar/search_sidebar.c" \
  "$root_dir/client/components/common/wekan_look.c" "$root_dir/tests/fakes/nuklear.c" "$root_dir/tests/fakes/nuklear_look.c" \
  "$root_dir/imports/ui/page_contract.c" "$root_dir/models/color.c" "$root_dir/models/model.c" \
  "$root_dir/models/list.c" "$root_dir/models/card.c" "$root_dir/models/wip_limit.c" -o "$test_dir/test"
"$test_dir/test"
echo "Search sidebar: title, field, Enter, results by title, opening a card, closing and negatives passed"
