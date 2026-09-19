# Checklist item input compatibility

`models/checklist_item_titles.[ch]` independently implements the behavior of
WeKan's `parseChecklistItemTitles` in `models/lib/checklistItemTitles.js`, pinned
at the compatibility source revision recorded by
`config/wekan-compat-inventory.json`
(`689a393841f08c3a020a4ef435b869b7641b21df`). It contains no copied JavaScript
and adds no dependency. The inspected source file SHA-256 is
`b0588ffc721b9d576f08c124ce34e3bc8449846ee1e9c075d24211b160d25fe8`.

The parser trims ECMAScript WhiteSpace and LineTerminator codepoints at both
ends of each candidate and omits blank candidates. With splitting disabled,
the whole trimmed input is one title and reverse has no effect. With splitting
enabled, only LF separates candidates; CR is trimmed at line ends and internal
CR is preserved. Reversal occurs after removing blank lines. Duplicate titles
remain duplicates. Unicode line/paragraph separators are whitespace for trim,
not additional split delimiters. U+180E, U+0085 and zero-width space are not
ECMAScript trim characters.

Intentional native boundaries are explicit: input is at most 16,384 UTF-8 bytes,
output at most 1,024 titles, each at most 128 bytes plus NUL, using the existing
checklist model's title capacity. The caller supplies output storage and an
explicit input length. Invalid UTF-8, embedded NUL, invalid boolean options,
length/capacity overflow and insufficient output capacity fail without writing
any titles; the returned count is zero. Empty input succeeds with zero titles.
NULL with zero length is treated as empty (the JavaScript non-string case).
Unlike JavaScript strings, lone UTF-16 surrogates cannot be represented by this
strict scalar UTF-8 API. Titles are never silently truncated.

The parser preserves internal whitespace/control characters in the canonical
manner. This is parsing, not authorization or model validation. Existing native
model and persistence validators still reject non-persistable titles and must
be called before mutation. In particular, an unsplit multiline title may parse
successfully and subsequently be rejected by the native single-line model.
The input, output and count memory ranges must not overlap.

The native editor now offers an explicit multiline checkbox when adding an
item. This uses the separate atomic batch action, with a tighter limit of eight
items and 1,031 raw UTF-8 bytes. Each parsed item still has a 128-byte title
limit. Blank lines disappear, duplicates remain, and appended items keep their
original order. A failure rejects the whole batch. The initial single-item
action remains available. See [atomic batch entry](checklist-batch.md) for the
transaction, input and test contract.

Checklist UI labels are derived from the same committed canonical language
catalog as other controls: `checklists`, `checklist`, `add-checklist`,
`add-checklist-item`, `rename`, `edit`, `complete`, and `checklist-count`.
The latter contains the literal example `(0/0)` and is not a formatting
placeholder. Dynamic progress should be rendered separately. The canonical
Jade item uses a checkbox with its item title; a translated "unfinished"
label is not invented where no such canonical key exists.
