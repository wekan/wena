# Local collapse preferences

`imports/preferences/collapse.[ch]` stores only native presentation preferences,
never canonical board/list/swimlane mutations. Files are independent for each
exact workspace path, local actor ID and board ID. The path helper takes the
workspace database directory and appends `wena-collapse-`, the SHA-256 of three
big-endian 32-bit length-prefixed UTF-8 identity fields, and `.prefs`. It reuses
Wena's existing MIT SHA-256 implementation. Long database filenames do not make
the preference filename exceed filesystem component limits. The complete path
must still fit `WENA_EXECUTABLE_PATH_CAPACITY`; failure disables this optional
preference rather than truncating a path. Different spellings of a workspace
path deliberately produce different identities; callers should reuse the same
validated desktop database path.

The strict version-one format is newline-terminated ASCII:

```
WENA-COLLAPSE 1
workspace <lowercase hexadecimal UTF-8 workspace path>
actor <validated native ID>
board <validated native ID>
swimlane <validated native ID>
list <validated native ID>
end
```

Zero or more swimlane entries precede zero or more list entries. Each collection
has the existing collapse model's maximum of 64 entries; IDs have its existing
64-byte limit. Duplicate IDs within a collection, extra fields, wrong order,
unknown versions, embedded NUL/control characters, incomplete lines, missing
terminators and oversized files are rejected. Both the hashed filename and
contents bind scope; copying a file to another actor/board filename does not
make its contents valid there. Same textual IDs in separate list and swimlane
namespaces are allowed.

Load returns `OK`, `MISSING` or `ERROR`. Missing files and errors leave the caller's
state unchanged. A successful load validates raw IDs but does not assume the
current hierarchy has been loaded: the desktop must then call the existing
`wena_board_collapse_sync` with its complete snapshot to prune missing, archived
or foreign IDs. A filtered view is not that snapshot. Loading/pruning is
read-only; startup must not overwrite saved preferences based on empty arrays
before the full snapshot exists.

Save checks existing content and scope, creates a mode-0600 exclusive `.tmp`
file, writes and flushes it, synchronizes its contents, and atomically renames
it over the destination. Malformed/differently scoped destinations are refused.
Existing temporary files or links are never truncated or deleted. Destination
symlinks and non-regular files are rejected. Reset atomically saves an empty
scoped state and changes caller state only after successful save. Write/flush/
rename errors preserve the previous file and remove only the temporary file
created by this call. Tests inject a real file-size-limit write failure and
check preservation and cleanup.

The containing directory is trusted local application storage, as for the
language sidecar. This is not protection against a malicious process concurrently
replacing directory entries or ancestor directories. POSIX reads use no-follow
where available and verify the opened regular file; Windows opens the reparse
point itself and rejects reparse/directory attributes. No arbitrary preference
path is accepted from remote card data. The Windows branch is implemented using
existing Win32 patterns but needs platform execution testing; the current suite
runs on POSIX. Parent-directory fsync is not performed, so atomic replacement
is not a guarantee that the rename survives every sudden power failure.

Concurrent local writers have atomic last-successful-write semantics, with
exclusive temporary-file collisions reported as errors. There is no remote
sharing or cross-process merge claim. Optional preference failure should display
the canonical generic error while leaving the workspace usable. The desktop
tracks changed states and should avoid retrying a failed disk write every frame.

## Desktop integration

The desktop derives the scoped preference path only after actor/board validation
and loading the complete board snapshot. It loads optional preferences, synchronizes
the collapse model against that snapshot, and records the observed state without
writing. Missing preferences start with expanded lists/swimlanes. Invalid or
unreadable preferences leave the board usable and show a contextual settings
error; no database implementation details are exposed in that message.

After rendering, a changed collapse state triggers one save attempt. The observed
state is updated even if saving fails, so an unchanged frame does not retry the
same failing write. Another collapse/expand action or the explicit canonical
Save button retries. Successful saving clears the error. Failed saving preserves
the current in-session presentation and the previous preference file. Save is a
retry, not an override of malformed/wrong-scope/symlink protection: these invalid
files must be corrected or removed outside this guarded adapter before saving
can succeed.

Smoke mode reads existing preference files but does not create, update or reset
them, and does not offer the retry button. This is read-only preference behavior;
smoke mode's database startup checks are a separate concern. Loading/pruning
alone never creates an empty preference file or erases previously collapsed IDs.

The pinned canonical catalog has no accurate message specifically for saving or
loading local collapse preferences. The generic `error-undefined` message remains
appropriate when shown with canonical `settings` and `collapse` context. Cloud/S3
storage settings errors describe different operations and are deliberately not
reused. All displayed strings continue to come from the committed canonical
catalog rather than a separately maintained translation list.
