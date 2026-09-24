# Upcoming Wena release

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
