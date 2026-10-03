# Upcoming Wena release

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
