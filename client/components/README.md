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
