# Canonical labels source contract

The native label work uses WeKan revision
`689a393841f08c3a020a4ef435b869b7641b21df` as its compatibility reference.
This document records source semantics and UI wording; it does not certify that
all listed behavior is implemented in Wena. Native storage and UI limits belong
in the corresponding feature implementation documentation.

## Provenance

All source paths below are relative to
[the pinned WeKan repository](https://github.com/wekan/wekan/tree/689a393841f08c3a020a4ef435b869b7641b21df).
The behavior was inspected locally. Wena reuses its own C89 adapters and imports
only canonical MIT translation data, not a second JavaScript implementation.

| Source | SHA-256 |
| --- | --- |
| `client/components/cards/labels.jade` | `7c5b602bdff039168352f11d504cd1a348af8ac946f5aa20cbc9293e53e78b4c` |
| `client/components/cards/labels.js` | `8922ddadcb78ef9185df342712476401a9d50430df126f6ca93e443e93787588` |
| `models/boards.js` | `a5e1586cdb83648f191c178dc9c8bbd22cfcd328fe40efcf1c443236cb0f9894` |
| `models/cards.js` | `e13f848359c601a999dc382ec55dd550f6a8db05c685873a3a03f0232ccc161f` |
| `models/metadata/colors.js` | `e7f63520073ac9ab42bf7636b19bce2046660393b944170047db02c50b7581ce` |
| `config/const.js` | `b42b831dc13b91e0a82ee5f55f8321ac8f94f3ab3faa0fbb249a3368d5094aa3` |
| `models/lib/contrastColor.js` | `d32aa53ce674f2a0e174d517d6f25b3ccbd9f5f65975cb160d2970cb3265dd81` |
| `server/models/boards.js` | `4fa356ec1eabc1e6497bf12382c591a15429ae2c572cd70dabb5e97fa614cea4` |
| `server/lib/accessibleCardOperations.js` | `c0d03b6ac72586303171e540d71c4560ae626e734d0991f0e60a6d8ac90f30c0` |

## Source behavior

- A board owns an ordered array of label IDs, optional names, and colors. A card
  selects a set of those IDs. Card labels are displayed in board catalog order,
  independently of the card assignment array order.
- Empty names are allowed, including labels whose UI input becomes empty after
  ECMAScript `String.trim`. The UI trims create and edit drafts. Upstream stored
  rows may have an absent name; a future import adapter must specify its mapping.
- Board helpers suppress an exact duplicate `(name, color)` pair. Different names
  can share a color and the same name can use different colors. A named color and
  an equivalent hexadecimal color remain distinct values; hexadecimal case is
  also preserved by the accepted stored value. Comparing visual RGB values must
  not accidentally merge catalog entries.
- The palette has 25 label colors, in `ALLOWED_COLORS` order, beginning with white.
  Creation chooses the first palette name not already used on that board; after
  every name is used it chooses the first again. The emergency no-palette fallback
  is green, which is different from the normal first palette entry.
- Stored colors accept exact named palette values or a six-digit hexadecimal
  string `#RRGGBB`, with either letter case. Three-digit shorthand is accepted by
  a display normalization helper but is not an accepted stored label color.
- Removing a board label removes its assignment from every card in the same
  board, including archived cards. The source update hook does this after the
  catalog update; native SQL should make both changes one atomic transaction.
- Accessible assignment accepts an explicit boolean enabled state, validates that
  the label belongs to the content board, and uses set semantics. Linked cards
  resolve their real source card and board. These source linked-card and access
  rules are not supplied merely by implementing ordinary native card labels.
- Label drag order changes the board catalog, not per-card assignment order.
  Linked-card editing still has source inconsistencies between popup board
  resolution and older edit/delete helpers. Wena should preserve exact scope and
  should not reproduce that route-dependent ambiguity.

## Canonical native UI contracts

The shared `imports/ui/page_contract.[ch]` owns keys and fallback text, and
`scripts/generate_ui_i18n.py` derives runtime values from the already pinned full
catalog. There is no manually maintained parallel translation list.

| Purpose | Canonical key |
| --- | --- |
| Label picker | `cardLabelsPopup-title` |
| Open create | `label-create` |
| Create heading | `createLabelPopup-title` |
| Edit heading and action | `editLabelPopup-title` |
| Label name | `name` |
| Palette | `select-color` |
| Hex input | `custom-color` |
| Create/save/cancel/close | `create`, `save`, `cancel`, `close` |
| Delete confirmation heading | `deleteLabelPopup-title` |
| Confirm deletion | `delete` |
| Affected assignment count | `cards` |

The upstream `label-delete-pop` body claims that deleting the label destroys its
history. Wena does not yet implement matching activity history, so that body is
not imported into the active native UI. The native confirmation instead uses the
canonical delete heading, the saved label name and color, and affected card count.
Opening confirmation or pressing Enter must not delete; deletion requires an
explicit confirmation button. No unverified translation wording is introduced.

## Shared text normalization

`models/text.[ch]` factors the existing checklist entry parser's strict UTF-8
scan and ECMAScript whitespace trimming into an allocation-free bounds helper.
It validates every input byte, rejects NUL, preserves output values on failure,
and returns empty bounds for all-whitespace input. U+180E, U+200B, and U+0085 are
not trim whitespace. Internal text is preserved exactly. This shared lexical
helper intentionally does not impose label/checklist control-character or byte
limits; each feature retains its own bounded model policy. Native label UI can
trim drafts without creating a dependency on the checklist feature.

`models/checklist_item_titles.[ch]` retains its public parser contract, including
LF-only splitting, blank removal, optional reversal, and all-or-nothing output.
The parser now delegates trimming and UTF-8 validation to the shared helper.
