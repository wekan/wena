#!/usr/bin/env sh
# One self-contained desktop executable for an Amiga-family target:
#
#   scripts/build_desktop_amiga.sh TARGET OUTPUT_EXECUTABLE
#
#   amigaos4-ppc   AmigaOS 4, PowerPC: ELF, SDL2 from the image
#   aros-amd64     AROS x86-64 (ABIv11): relocatable ELF, SDL2 2.32.10 with AROS's port
#   aros-i386      AROS x86 32-bit (ABIv0, AROS One and the other i386
#                  distributions): the same, built with deadwood2's alt-abiv0
#   aros-arm64     AROS AArch64 (raspi-aarch64: Raspberry Pi 3/4/5): the same
#   amigaos-m68k   AmigaOS 3.x, 68040 + FPU + RTG: HUNK, diasurgical's SDL2
#   amigaos-m68k-aga
#                  the same for AGA without RTG: an 8-bit 640x512 screen,
#                  SDL's AGA path with scripts/patches/sdl2-amigaos3-aga.patch
#
# Runs on the host: it fetches the pinned, checksum-verified sources
# (config/release-dependencies.json) into .tools/cache and then compiles
# inside the pinned amigadev/crosstools image ("docker-images" in the same
# file), with the repository mounted at /work, through
# scripts/build_desktop_amiga_container.sh. SDL2 and SQLite are linked
# statically: the executable needs only what the operating system ships.
set -eu
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if [ "$#" -ne 2 ]; then
  echo "Usage: scripts/build_desktop_amiga.sh TARGET OUTPUT_EXECUTABLE" >&2
  exit 2
fi
target=$1
output=$2
# The AROS images are published for amd64 only; the others are multi-arch and
# run natively on an arm64 host.
platform=
case "$target" in
  amigaos4-ppc) sources="sqlite" ;;
  aros-amd64|aros-i386|aros-arm64)
    sources="sqlite sdl2 sdl2-aros-patch sdl2-aros-static sdl2-aros-intern"; platform=linux/amd64 ;;
  amigaos-m68k|amigaos-m68k-aga) sources="sqlite sdl2-amigaos3" ;;
  *)
    echo "unknown Amiga desktop target: $target" >&2
    exit 2
    ;;
esac
command -v docker >/dev/null 2>&1 || { echo "Docker is required to build $target" >&2; exit 1; }
image=$(python3 - "$root_dir/config/release-dependencies.json" "$target" <<'PY'
import json, re, sys
pins = json.load(open(sys.argv[1], encoding="utf-8"))
images = [entry["image"] for entry in pins["docker-images"] if entry["target"] == sys.argv[2]]
if len(images) != 1 or not re.fullmatch(r"[a-z0-9./-]+:[a-z0-9._-]+@sha256:[0-9a-f]{64}", images[0]):
    sys.exit("no image pinned by digest for " + sys.argv[2])
print(images[0])
PY
)
# The cache must be inside the repository: only /work is mounted.
cache="$root_dir/.tools/cache"
for name in $sources; do
  python3 "$root_dir/scripts/fetch_release_dependency.py" "$name" "$cache" > /dev/null
done
sh "$root_dir/scripts/check_desktop_sources.sh"
# What needs Python or patch is done here, so an image needs only its
# compiler: SQLite's amalgamation, and AROS's SDL2 port applied.
python3 "$root_dir/scripts/prepare_amiga_sources.py" "$target" "$cache" \
  "$root_dir/.tools/release/$target/sources" > /dev/null
built=".tools/release/$target/wena-desktop"
rm -f "$root_dir/$built"
docker run --rm ${platform:+--platform "$platform"} \
  --user "$(id -u):$(id -g)" --env HOME=/tmp --env WENA_SOURCES_CHECKED=1 \
  --volume "$root_dir:/work" --workdir /work \
  "$image" sh scripts/build_desktop_amiga_container.sh "$target" "$built"
mkdir -p "$(dirname -- "$output")"
cp "$root_dir/$built" "$output"
if command -v file >/dev/null 2>&1; then file "$output"; fi
