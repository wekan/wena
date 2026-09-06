#!/usr/bin/env sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

if test "$#" -eq 0; then
  if ! test -t 0; then
    echo "Interactive menu requires a terminal; use --list or a named command." >&2
    exit 2
  fi
  exec python3 "$root_dir/scripts/wena.py" menu
fi

# Named commands never prompt or pause after a long-running process.
exec python3 "$root_dir/scripts/wena.py" "$@"
