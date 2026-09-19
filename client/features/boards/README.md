# Board settings

`settings_store.[ch]` defines a small validated snapshot without a SQLite or
Nuklear dependency. `settings.[ch]` adapts loading and explicit guarded writes to
the shared SQLite persistence path. `settings_panel.[ch]` presents a local draft
with Save and Cancel; the desktop owns opening it and rendering card summaries.

The current option is canonical WeKan
`allowsChecklistCountBadgeOnMinicard`, shown using
`checklist-count-on-minicard`. Its default is false. Schema v6 stores the explicit
boolean in `board_settings`; an absent row means false, while a missing table
fails loading. Saving a changed choice advances the board revision once.

Rendering and toggling the checkbox do not write. Save uses the loaded board
revision, including for unchanged values; stale versions reject. Cancel, Close
and focused Escape discard the draft. Enter never submits the checkbox. Read-only
mode presents the saved value without an editable widget or Save control. If a
write succeeds and reloading fails, only Refresh and Close remain available so
the accepted request cannot be submitted again.

The panel keeps a failed draft intact and publishes newly loaded values only
after snapshot validation. `test_board_settings_panel.sh`,
`test_nuklear_board_settings.sh` and `test_board_settings_panel_sqlite.sh` cover
fake callback behavior, real Nuklear interactions and complete SQLite persistence
respectively. Minicard projection and the remaining board options have separate
ownership and must not be inferred from this one stored setting.

## Presentation cache lifecycle

`presentation` owns the board label badge snapshot and optional checklist count
projection. It factors the desktop's cache/callback lifecycle into a tested
feature service; rendering still consumes the same snapshots and performs no
queries. Initialization allocates both projections and attempts their initial
reads. Resource success is separate from read success, which is represented by
explicit validity/error flags and allows a visible retry.

A single `sqlite3_total_changes64` call after editor processing detects writes on
the local connection. Only pending projections reload, at most once each per
poll. Unchanged polls execute zero SQL. The counter can also advance for changes
subsequently rolled back; one harmless reload is preferable to inferring commit
success from that counter. Committed mutation callbacks return success before
presentation reloads, so a failed read never causes the caller to repeat an
already committed mutation.

After a read failure the relevant badges are hidden, the error flag remains set
and automatic per-frame retries stop. Setting the relevant pending flag requests
an explicit read-only retry. A successful panel read can also detect a changed
board revision and request refresh. External connections are not continuously
polled; their card writes require an explicit refresh, and settings changes are
observed when the board settings panel is opened. This local presentation cache
is not a live synchronization mechanism.

The opt-in setting and enabled count projection are separate atomic reads. Their
board versions must agree before publication, preventing counts loaded after an
intervening settings commit from being displayed with the earlier option value.
When disabled, no checklist-count query runs.

`tests/test_board_presentation.sh` verifies 500 unchanged polls with zero traced
SQL statements, initial opt-out, committed label/checklist changes, failed
reloads, explicit retries, rolled-back counter changes, an external settings
change and a forced commit between the two reads, plus close/reopen ownership.
