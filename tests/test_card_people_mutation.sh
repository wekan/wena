#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
export WENA_PEOPLE_TEST_SOURCE=card_people_mutation_test.c
exec sh "$root_dir/tests/test_card_people_store.sh"
