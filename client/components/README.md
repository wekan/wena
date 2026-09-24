# Components

This directory mirrors `client/components/` in Meteor WeKan. Board, swimlane, list,
card, sidebar, user, activity, settings, import, and common component modules belong
in matching subdirectories and render through Nuklear without owning application
state.

`common/paginated_table.[ch]` is the shared template for tabular or single-column
paged content. Supply a `WenaTableView` with immutable row count, column headings,
row height, empty/error text, and a callback that renders one row. Initialize a
`WenaTableState` per view with a page size of 1–256. The component clamps pages
when rows disappear, renders only the current page, and reports the first row
intent with its absolute snapshot index. Resolve that index to an exact ID and
revision in the feature before applying a mutation; never retain an index across
reloads. Reset the page on scope/filter/sort changes. Separate simultaneous tables
with caller-owned Nuklear windows or groups.

The table owns no database, data, selection or allocation. This lets unrelated
features use the same navigation and error/empty handling without conflating
permissions or mutations. Activities, members, labels and archives already share
it through the sidebar's text-row adapter. Future administration tables should
provide their own row callback rather than copy pagination loops. Ordinary
Nuklear labels and controls remain live text; SVG is for scalable artwork/themes.

`common/card_section.[ch]` is the shared collapse control for card sections.
The owner supplies a validated actor/board preference snapshot and a frame-local
intent slot. Both minicards and opened cards use `checklist-<id>` for a checklist,
matching WeKan's profile key, so they share state rather than keep separate flags.
The control performs no I/O: the feature consumes a captured preference revision
after rendering, then refreshes the snapshot. Read-only/error states draw inert
controls. Reuse this control and `models/card_section` for future card sections.

`forms/text_form.[ch]` supplies a single-line draft editor and Save/Cancel
controls, with an optional multiline mode whose Enter inserts a newline. Callers
own the buffer, byte limit, validation and persistence. Escape
wins over Enter within the focused host window; the component only reports an
intent. Existing title editors share its keyboard policy. Checklist previews use
the same form for list names, item names and item creation, with exact IDs and
captured revisions carried by `features/checklists/inline_edit`. Submit after
rendering: failures retain the draft, successful writes consume it before reload.

`features/checklists/entry_form.[ch]` adapts the general text form to checklist
entry in both opened cards and minicards. It owns the single/batch toggle and
parsed item-count preview. Both hosts call the existing canonical title parser
and atomic checklist mutation; neither duplicates parsing or line-splitting.

`common/reorder_drag.[ch]` is the reusable same-collection drag handle. Wrap
participating rows in one begin/end pair; provide each row's exact ID, ordinal,
collection scope and revision. A seven-pixel gesture produces a single source/
target intent on release. Escape, lost/hidden source rows, outside/cross-scope
drops, changed revisions and read-only rows cancel. The host owns rendering,
validation and the post-frame mutation. Its existing explicit Move controls
remain the keyboard alternative.

The implementation uses the pinned Nuklear input/widget APIs, checked against
the [upstream input documentation](https://immediate-mode-ui.github.io/Nuklear/Input.html)
and local clipping/focus implementation; it adds no dependency. Checklist and
item adapters share this control and the existing guarded reorder transaction.
Ordinals include hidden siblings, while invisible rows cannot become drop targets.

Explicit destination zones reuse `wena_reorder_drag_drop`. The feature decides
which destinations to offer and records the destination IDs/revisions; begin/end
still requires a visible valid source. Checklist transfers append to another
active card, and item transfers append to another checklist on the same board.
These use existing atomic transfer operations; an ordinary sibling row remains a
same-collection reorder target rather than silently changing transfer semantics.
