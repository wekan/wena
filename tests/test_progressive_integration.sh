#!/usr/bin/env sh
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
"$root_dir/build.sh" tests html4-render
"$root_dir/build.sh" tests capability
"$root_dir/build.sh" tests regions
"$root_dir/build.sh" tests http-serving
