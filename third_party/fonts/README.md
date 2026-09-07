# Trusted native font

`RobotoStatic-Regular.ttf` is copied unchanged from the Android Open Source
Project's `platform/external/roboto-fonts` repository at commit
`5d982bb4526f3e8a438a776619c060e1daf2f18f`. The exact source URL is:

https://android.googlesource.com/platform/external/roboto-fonts/+/5d982bb4526f3e8a438a776619c060e1daf2f18f/RobotoStatic-Regular.ttf

The font's own name table identifies `Version 2.138; 2017`, copyright 2011
Google Inc. All Rights Reserved., licensed under Apache License 2.0.
The repository's METADATA version 3.005 describes its separate variable font;
it is not this static font's version. `LICENSE-Roboto.txt` preserves the
upstream NOTICE containing the full Apache 2.0 license. `provenance.json`
pins source and license SHA-256 hashes. Wena's application remains MIT; the
font is distributed under its own compatible Apache 2.0 license.

The pinned WeKan source also contains an older Roboto WOFF, but its character
map lacks Greek capital Omega. This Android static font has that glyph and
avoids a new WOFF conversion pipeline. This choice does not claim parity
with WeKan's active CSS: its inspected Roboto font-face block is commented.

Run `python3 scripts/generate_native_font.py` to regenerate the C byte array
and ranges; `--check` verifies exact bytes, bounded TrueType table directories,
table checksums and deterministic output. The source TTF is kept for offline,
reproducible generation; no TTF file is read at application runtime. The C
array is a mechanical byte encoding, not a modification of the font.

The native helper loads only this trusted embedded asset. It selects existing
glyphs in Latin, Greek, Cyrillic, Latin/Greek extended and punctuation blocks.
It tests Finnish accents, Greek Omega/alpha and Cyrillic letters using a real
Nuklear atlas bake. Unsupported characters use `?`. It provides neither CJK
coverage nor Arabic shaping, bidirectional layout or all 246 languages' glyphs.
The desktop retains the Nuklear default-font fallback when adding the font
returns NULL. This does not promise recovery from every out-of-memory failure
inside Nuklear's allocator paths. Arbitrary font paths remain unsupported:
the upstream font parser explicitly does not guarantee untrusted-input safety.
