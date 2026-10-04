#!/usr/bin/env sh
# Attach the files a build job made to the release, the moment that job has
# built and checked them, so a release fills in as its builds finish rather
# than when the slowest one does. Used by every build job of
# .github/workflows/release-all.yml (and so by release-all-missing.yml).
#
#   GH_TOKEN=... sh scripts/attach_release_files.sh TAG DIRECTORY
#
# Each file in DIRECTORY replaces one of the same name (--clobber), so a
# rebuilt file updates the release; SHA256SUMS is never attached here - the
# workflow's last job writes it over everything the release then has.
set -eu
if [ "$#" -ne 2 ] || [ -z "$1" ] || [ ! -d "$2" ]; then
  echo "Usage: sh scripts/attach_release_files.sh TAG DIRECTORY" >&2
  exit 2
fi
tag=$1
directory=$2
repository=${GITHUB_REPOSITORY:?GITHUB_REPOSITORY is not set}
set --
for file in "$directory"/*; do
  [ -f "$file" ] || continue
  case "$(basename "$file")" in
    SHA256SUMS) continue ;;
  esac
  set -- "$@" "$file"
done
if [ "$#" -eq 0 ]; then
  echo "nothing to attach in $directory" >&2
  exit 1
fi
attempt=1
while [ "$attempt" -le 3 ]; do
  if gh release upload --repo "$repository" "$tag" "$@" --clobber; then
    for file in "$@"; do echo "attached $(basename "$file") to $tag"; done
    exit 0
  fi
  echo "::warning::Attaching to $tag, attempt $attempt/3 failed"
  attempt=$((attempt + 1))
  sleep 10
done
echo "::error::Could not attach $* to $tag"
exit 1
