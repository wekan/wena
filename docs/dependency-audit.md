# Native desktop dependency review

Review date: 2026-09-07. This is a scoped review of dependencies actually used by
the local desktop, not a vulnerability-free certification or a complete advisory
feed. The Nuklear pin and system libraries were not changed. A trusted Apache-2.0
font asset was added with pinned provenance and its required notice.
A separately built SQLite test runtime is documented below; it is not vendored
or bundled with the application.

## Provenance and deployment

| Dependency | Wena input | License and delivery |
| --- | --- | --- |
| Nuklear | Git submodule `e3e18dc1e4d3de935095d372aaa211f12183befb`; header reports 4.13.3 | MIT alternative A; compile the pinned header and SDL renderer backend; retain upstream LICENSE |
| SDL2 | System headers and runtime; pinned renderer requires at least 2.0.22 | zlib; dynamically linked system dependency |
| SQLite | System headers and runtime | Public domain; dynamically linked system dependency |
| Roboto Static Regular | Android commit `5d982bb4526f3e8a438a776619c060e1daf2f18f`, exact TTF hash | Apache-2.0; embedded bytes and retained NOTICE, see `third_party/fonts/README.md` |

`config/dependencies-lock.json` pins SHA-256 for the Nuklear header, renderer
backend and license. `scripts/check_dependencies.py` verifies those bytes and,
when Git metadata exists, the superproject's submodule gitlink. Source archives
can still verify file hashes without Git metadata. A checksum detects accidental
or unreviewed input changes; it is not a vulnerability scan or independent proof
that a maliciously changed lock is trustworthy.

The [Nuklear release page](https://github.com/Immediate-Mode-UI/Nuklear/releases)
currently identifies 4.13.3 as its latest tagged release. Wena pins a later
specific upstream commit rather than a moving branch. This review found no
confirmed fix that justifies replacing the already-tested pin. The GitHub
advisory page could not be retrieved during the review, so it supplies no evidence
about the existence or absence of advisories.

The [SDL2 installation documentation](https://wiki.libsdl.org/SDL2/Installation)
identifies SDL2's zlib license and system-package/dynamic-linking workflow. SDL3
is not an interchangeable upgrade for this SDL2 renderer. SQLite documents its
[public-domain status](https://www.sqlite.org/copyright.html). No GPL source was
introduced. Packaged desktop ELF requirements still govern actual platform and
glibc compatibility; this document does not make a host-built artifact portable.

## Concrete SQLite WAL concern

Wena enables WAL and FULL synchronization in `server/sqlite_storage.c`. SQLite's
[WAL-reset bug documentation](https://sqlite.org/wal.html#walresetbug) describes
rare corruption when separate connections concurrently write or checkpoint the
same WAL database. The upstream fix is in 3.51.3 and later, with documented
backports in the 3.44 and 3.50 branches beginning at 3.44.6 and 3.50.7. Wena's
optimistic versions and idempotency keys do not repair this engine-level race.

Use a maintained system SQLite package with a confirmed WAL-reset fix before
concurrent production access. Check the distributor's package revision and
advisory: an older upstream version string alone cannot establish whether a
vendor backported the fix. This review does not change journal mode. The separate
fixed test runtime below is used for subsequent validation. A single-process test pass does not
reproduce or rule out the timing-sensitive bug.

The inspected host reported SDL2 headers/runtime 2.30.0 and SQLite
headers/runtime 3.45.1 with source ID
`2024-01-30 16:01:20 e876e51a0ed5c5b3126f52e532044363a014bc594cfefa87ffb5b82257ccalt1`.
That upstream version does not demonstrate the WAL-reset fix; this environment's
distributor backport status was not verified. These observations describe the
review host, not libraries subsequently loaded on another machine.

SQLite's [upstream CVE analysis](https://sqlite.org/cves.html) distinguishes bugs
requiring arbitrary SQL, crafted FTS5 data, optional extensions, or unsafe C API
arguments. Wena uses bound application values and fixed statements, and does not
currently expose an arbitrary-SQL console or use the zipfile/session extensions.
That narrows some entry points; it does not establish that opening an untrusted
database is safe. SQLite's [release history](https://sqlite.org/changes.html)
should be reviewed alongside distributor fixes when choosing deployment packages.

## Font input boundary

The pinned Nuklear header's embedded font parser explicitly disclaims security
for untrusted font files (see `third_party/nuklear/nuklear.h`, the font-parser
section). Desktop loads a trusted embedded Roboto static font, with Nuklear's
embedded default as fallback. No arbitrary runtime font paths are accepted.
The offline generator validates the pinned TTF tables, checksums and byte ranges;
real atlas tests cover selected Latin, Greek and Cyrillic glyphs. This does not
establish universal coverage, bidirectional shaping or recovery from every font
atlas allocation failure: upstream allocation internals can fail before returning
to the wrapper. Accepting user-supplied fonts needs a separate parser decision.

## Reproduce the checks

```sh
python3 scripts/check_dependencies.py
python3 tests/test_dependencies.py
python3 scripts/check_dependencies.py --runtime
```

The optional runtime command builds and executes a small C89 host probe using
`CC`, `sdl2-config`, and the same system SQLite linkage. It prints header/runtime
versions, SQLite source ID, compile-time threading capability, and whether the
version belongs to a documented fixed upstream WAL branch. A false value for
that branch check means distributor review is needed, not a proven vulnerability.
The probe creates no database and initializes no graphical display. Keep its
output separate from the ELF-derived package manifest.

Before a future Nuklear pin change: identify the exact upstream fix, update the
submodule and provenance lock together, run strict C89 compilation plus real
Nuklear input/selector/theme tests, SDL event/clipboard tests, sanitizers, and the
desktop smoke/package checks. Do not substitute a major GUI/database dependency
merely to obtain a newer version number.

## Inspect a deployed desktop process

```sh
./wena-desktop --dependency-info
```

Use this sole option before opening a workspace or initializing a video driver.
The executable emits `format=wena-dependencies-v1` followed by stable `key=value`
lines containing its SDL2/SQLite header versions and actually loaded runtime
versions, SQLite source ID and threading setting. Controls and percent signs
inside values are encoded as `%HH`; ordinary source-ID spaces remain literal.
The `sqlite_wal_reset_status` is either `known_fixed_upstream_version` or
`unknown_distribution_backport_status`. The latter is a request for distributor
backport verification, not an assertion that the loaded library is vulnerable.
This option is diagnostic only and does not block normal use based on version.
A package-build snapshot records the libraries loaded during packaging; run the
option again on the destination machine after system-library upgrades.

## Fixed SQLite test runtime

After the initial host review, SQLite **3.51.3** was built in a temporary directory
outside the repository for retesting. The official
[3.51.3 release record](https://sqlite.org/releaselog/3_51_3.html) identifies the
WAL-reset fix, source ID and amalgamation SHA3-256. The downloaded `sqlite3.c`
matched that published SHA3-256 before compilation:

- URL: `https://www.sqlite.org/2026/sqlite-amalgamation-3510300.zip`
- Downloaded ZIP SHA-256: `acb1e6f5d832484bf6d32b681e858c38add8b2acdfd42ac5df24b8afb46552b4`
- Published `sqlite3.c` SHA3-256: `32d5424f97e0a7fc5ed2f6335afbb58be4e0298bd7117a34e39d345ff13d859e`
- Runtime source ID: `2026-03-13 10:38:09 737ae4a34738ffa0c3ff7f9bb18df914dd1cad163f28fd6b6e114a344fe6d618`

The build uses `SQLITE_THREADSAFE=1`, shared-library output, and SQLite's other
amalgamation defaults. The optional FTS5, zipfile, and session extensions were not
enabled. This is a verified WAL-fix test baseline, not a claim that 3.51.3 is the
latest release or that every possible SQLite issue is fixed in it. The staged
runtime passed the C89 dependency probe and Wena SQLite persistence suite before
being made available to the broader test/build run. The original system library
was left unchanged. Final full-suite results belong in the run's test report.

Reproduce with a separate temporary runtime (from the repository root; SDL2
headers/runtime and a C compiler must already be available):

```sh
wena_sqlite_test_dir=$(mktemp -d)
curl -fL https://www.sqlite.org/2026/sqlite-amalgamation-3510300.zip \
  -o "$wena_sqlite_test_dir/sqlite.zip"
python3 - "$wena_sqlite_test_dir" <<'PY'
import hashlib
from pathlib import Path
import sys
import zipfile
root = Path(sys.argv[1])
with zipfile.ZipFile(root / 'sqlite.zip') as archive:
    source = archive.read('sqlite-amalgamation-3510300/sqlite3.c')
    assert hashlib.sha3_256(source).hexdigest() == (
        '32d5424f97e0a7fc5ed2f6335afbb58be4e0298bd7117a34e39d345ff13d859e')
    for name in ('sqlite3.c', 'sqlite3.h', 'sqlite3ext.h'):
        (root / name).write_bytes(archive.read('sqlite-amalgamation-3510300/' + name))
PY
cc -O2 -fPIC -shared -DSQLITE_THREADSAFE=1 -Wl,-soname,libsqlite3.so.0 \
  "$wena_sqlite_test_dir/sqlite3.c" -o "$wena_sqlite_test_dir/libsqlite3.so.0" \
  -ldl -lpthread -lm
ln -s libsqlite3.so.0 "$wena_sqlite_test_dir/libsqlite3.so"
C_INCLUDE_PATH="$wena_sqlite_test_dir${C_INCLUDE_PATH:+:$C_INCLUDE_PATH}" \
LIBRARY_PATH="$wena_sqlite_test_dir${LIBRARY_PATH:+:$LIBRARY_PATH}" \
LD_LIBRARY_PATH="$wena_sqlite_test_dir${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
  python3 scripts/check_dependencies.py --runtime
```

Use those same three environment variables for the desired SQLite suites and
desktop build/smoke run. `LIBRARY_PATH` controls the link step;
`LD_LIBRARY_PATH` makes the executed test load the temporary fixed library.
Neither turns a distributed executable into a package with bundled SQLite:
a recipient must still provide a suitable maintained system runtime. Never copy
through a temporary development-library symlink that points into `/usr/lib`;
replace only the temporary link itself when changing a test setup.
