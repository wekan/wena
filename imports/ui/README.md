# Shared UI contract

`page_contract` is the semantic boundary shared by the native Nuklear client and
the optional server's Legacy HTML4 renderer. It follows Meteor WeKan's canonical
Legacy HTML4 route headings, control/domain-operation names, board/item colors,
single-content-table baseline, ASCII controls, and natural tab order. Text entries
store canonical WeKan i18n keys; fallback labels are only bootstrap diagnostics and
must not become another translation catalog.

HTML rendering, ROOT_URL joining, sessions, and mutation security remain server
responsibilities. They consume this contract instead of redefining page semantics.
