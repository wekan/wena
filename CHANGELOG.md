# Upcoming Wena release

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
