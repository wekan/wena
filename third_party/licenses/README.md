# Licenses of what is linked into the release executables

Copied verbatim from the pinned sources in `config/release-dependencies.json`,
so `scripts/generate_notices.py` can compile them into every Wena executable
(`wena --licenses` prints them):

- `SDL2-LICENSE.txt`: `LICENSE.txt` of SDL2-2.32.10.tar.gz (Zlib).
- `SQLITE.txt`: the public-domain dedication at the top of `sqlite3.h` of the
  pinned amalgamation.
