#!/usr/bin/env python3
"""The link flags each release system gets in scripts/build_desktop_release.sh.

Haiku's SDL video is C++ (BWindow, std::vector): its static libSDL2.a needs
libstdc++ after it, which gcc does not add when it links C. Without it the
haiku-amd64 release stopped on std::__throw_length_error. No other system's
SDL is C++, so none of them links the C++ runtime."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = (ROOT / "scripts" / "build_desktop_release.sh").read_text(encoding="utf-8")


def cases():
    body = SCRIPT[SCRIPT.index('case "$target" in'):]
    body = body[:body.index("\nesac")]
    return dict(re.findall(r"^  ([a-z0-9*|_-]+)\)\n(.*?)\n    ;;", body, re.M | re.S))


def main():
    found = cases()
    haiku = found["haiku-*"]
    assert re.search(r'ldflags="[^"]*-lstdc\+\+', haiku), haiku
    # It goes in WENA_LDFLAGS, which build_desktop.sh puts after the SDL archive.
    build = (ROOT / "scripts" / "build_desktop.sh").read_text(encoding="utf-8")
    assert build.index("$WENA_SDL_LIBS") < build.index("$WENA_LDFLAGS")
    assert "WENA_LDFLAGS=$ldflags" in SCRIPT
    # Negative: the others stay C only.
    for pattern, text in found.items():
        if pattern != "haiku-*":
            assert "-lstdc++" not in text, pattern
    print("release link flags: Haiku links libstdc++ after SDL, no other system does")


if __name__ == "__main__":
    main()
