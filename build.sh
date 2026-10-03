#!/usr/bin/env sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

# Everything else a build needs (compilers, SDKs, the Android NDK, Docker) is
# installed by scripts/toolchain.py when that build runs; it needs Python 3.
if ! command -v python3 >/dev/null 2>&1; then
  if test "$(id -u)" -eq 0; then sudo=; else sudo=sudo; fi
  if test "$(uname -s)" = Darwin; then
    if command -v brew >/dev/null 2>&1; then
      echo "Installing Python 3: brew install python" >&2
      brew install python
    else
      # The Command Line Tools include python3.
      echo "Python 3 is missing: run 'xcode-select --install' or install Homebrew from https://brew.sh" >&2
      exit 1
    fi
  elif command -v apt-get >/dev/null 2>&1; then
    echo "Installing Python 3: apt-get install python3" >&2
    $sudo apt-get update
    $sudo apt-get install --yes python3
  elif command -v dnf >/dev/null 2>&1; then
    echo "Installing Python 3: dnf install python3" >&2
    $sudo dnf install --assumeyes python3
  else
    echo "Python 3 is missing; install it with this system's package manager." >&2
    exit 1
  fi
fi

if test "$#" -eq 0; then
  if ! test -t 0; then
    echo "Interactive menu requires a terminal; use --list or a named command." >&2
    exit 2
  fi
  exec python3 "$root_dir/scripts/wena.py" menu
fi

# Named commands never prompt or pause after a long-running process.
exec python3 "$root_dir/scripts/wena.py" "$@"
