# Upcoming Wena release

<details>
<summary>Release menu option and a desktop release workflow; the desktop builds with gcc again</summary>

- Menu option 3) Release (`build.sh release [desktop|bootstrap|all] [TAG]`)
  starts the GitHub workflows with `gh`: `release-desktop.yml` for the desktop,
  `release-all.yml` for the bootstrap targets, or both. It never pushes:
  commits not yet on GitHub stop it with the push command, since the workflow
  would build older code. Tests, Server and Tools move to 4, 5 and 6.
- `release-desktop.yml` builds `wena-desktop` on native Linux amd64/arm64 and
  macOS arm64/amd64 runners, smoke-tests it headless twice, packages it with
  `scripts/package_desktop_release.py` (deterministic tar.gz with licenses,
  provenance, a README of the system libraries and SHA256SUMS, plus a
  `.sha256`) and attaches the files to the given or newest release.
- gcc 13 rejected 27 one-line `if (...) a; b;` statements under
  `-Werror=misleading-indentation`, so the desktop and five suites did not
  build on Ubuntu 24.04; each is now one statement per line. A WeKan source
  lookup no longer raises when the checkout is one folder deep. On Ubuntu
  24.04, 163 suites pass; `capability-runtime` needs Node.js and the LD_PRELOAD
  part of `desktop` still fails (it never built there before).
- Tests: release dispatch with gh and git replaced (every refusal starts
  nothing, retries, both workflows, the tag only for the desktop), packaging
  determinism, contents, checksums and negatives, and the workflow's targets.

Thanks to xet7.

</details>

<details>
<summary>Drag the bar below a swimlane to change its height</summary>

- Each expanded swimlane has a bar below it. Dragging it up or down resizes
  the lane live, with the up-down resize cursor; release keeps the height,
  Escape cancels. Heights are clamped to 160-2000 pixels, the default 360
  needs no entry, and the lists inside grow with the lane.
- Heights are saved per actor and board with collapsed lanes and lists. The
  preferences file is version 2 exactly when it holds heights, so version 1
  files keep loading; heights of lanes the board no longer has are dropped.
- Tests: a real Nuklear drag test (press, drag, release, Escape, both clamps,
  back to the default, a collapsed lane and a layout without the bar), and
  preference round trips with strict negatives for every malformed height
  line, a full file, and pruning.

Thanks to xet7.

</details>

<details>
<summary>Fix the desktop crash when the mouse reaches a list or swimlane drag handle</summary>

- 42 sources included `<nuklear.h>` without the `NK_INCLUDE_*` options that
  `sdl_nuklear.h` set before compiling Nuklear itself. Those options add fields
  to `struct nk_context`, so the drag handles read `context->current` at another
  offset than Nuklear wrote it (0x4920 instead of 0x4a10), got NULL and crashed
  with SIGSEGV, as three macOS crash reports and the debug log showed.
- The options now live only in `client/platform/nuklear_options.h`, included
  before `<nuklear.h>` in every source and test, and the tests that compiled
  Nuklear with two or three of them now use the same set. The narrow clang
  C23-extension silence for Nuklear's alignof moved there too, so 24 Nuklear
  suites build and pass on macOS again.
- `tests/test_nuklear_options.py` (suite `nuklear-options`) fails when any of
  the 100 units includes Nuklear without the options first or sets an option
  elsewhere, with negative cases for both.
- With no workspace named, also when only `--smoke` or `--language` is given,
  the desktop opens the default board; the desktop suite tests its creation,
  reopening and a refused relative `WENA_DATABASE`.

Thanks to xet7.

</details>

<details>
<summary>Debug log for every desktop run, and the desktop opens its board when double-clicked</summary>

- Each run writes `.tools/log/wena/YYYY-MM-DD_HH-MM-SS/desktop.log`: the
  arguments, the board opened, the source line of any startup failure with
  SDL's error, the window closing, the exit status, and a fatal signal, written
  with async-signal-safe calls before the default action, so a crash is visible.
  `build.sh run` adds `run.log` with everything the app printed and whether it
  exited or was killed by a signal. `WENA_LOG_DIR` chooses another folder.
- Started without arguments, as a double-click does, `wena-desktop` opened
  nothing and printed that it needs a database, actor and board. It now opens
  the same local board as Run, created on the first run.
- Tests cover the log folder and default board rules with their negative
  cases, a written line and a recorded crash in a child process, and run.log's
  output, exit code and signal.

Thanks to xet7.

</details>

<details>
<summary>Run opens the native Nuklear desktop, and the desktop builds on macOS again</summary>

- Menu option 2) Run and `build.sh run` open `dist/desktop/wena-desktop` on a
  local board, created on the first run in the user's data folder or in
  `WENA_DATABASE`, instead of the bootstrap binary that only prints its name.
  Given arguments go to the desktop unchanged.
- The desktop did not compile with Apple clang 21: `SDL2/SDL.h` was not found
  through `sdl2-config`, `mkdtemp` was hidden by `_POSIX_C_SOURCE`, and the pinned
  Nuklear's alignof is reported as a C23 extension. The desktop now includes
  `SDL.h` like the other sources, defines `_DARWIN_C_SOURCE` on macOS, and
  silences only that warning around the two Nuklear headers on a clang that
  knows it. The `nuklear` and `sdl-text-input` suites pass on macOS again.
- Tests cover the desktop path per platform, the data folder per system and
  `WENA_DATABASE`, first-run creation and reopening, both refusals and passed
  arguments; the desktop smoke test passes for a created and a reopened board.

Thanks to xet7.

</details>

<details>
<summary>Add a Run option to the build menu</summary>

- `build.sh` and `build.bat` menu option 2) Run starts the binary that 1) Build
  wrote for the current computer, `dist/<target>/wena` or `wena.exe`. Tests,
  Server and Tools move to options 3, 4 and 5. The same is `run [ARGS...]` as a
  named command, which passes its arguments on.
- A missing or non-executable binary is refused with how to build it. Tests
  cover the file name per platform, both refusals, a real run with its
  arguments and exit code, and the menu order.

Thanks to xet7.

</details>

<details>
<summary>Preserve person assignments when moving cards between boards</summary>

- Reuse shared member filtering and strict SQLite readers in single-card and
  bulk cross-board transfers. Retain members active on the destination board,
  preserve assignees, and keep their original ordering positions.
- Verify card metadata, both board rosters, and person/actor data throughout the
  transfer transaction. Roll back ignored writes, partial transfers, and changes
  caused by later writes or triggers. Keep foreign-key enforcement enabled.
- Add regression coverage for inactive and absent destination members, empty and
  full 2048-person sets, roster overflow, deferred parent updates, rollback and
  reopening. Update the roadmap; native person capture adapters, roster
  management and person controls remain unfinished.

Thanks to xet7.

</details>
