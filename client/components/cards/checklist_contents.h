#ifndef WENA_CHECKLIST_CONTENTS_COMPONENT_H
#define WENA_CHECKLIST_CONTENTS_COMPONENT_H
#include "../../features/checklists/summary.h"
#include "../../features/checklist_store.h"
#include "../common/card_section.h"
#include "../../features/checklists/inline_edit.h"
struct nk_context;
/* Read-only preview from an immutable validated snapshot. Explicit title clicks
 * return OPEN_CHECKLISTS for the caller's exact card. No SQL or writes here. */
unsigned int wena_checklist_contents_render(struct nk_context *context,
    const WenaChecklistBoardContents *contents, const WenaCard *card,
    int board_default);
/* With a non-NULL intent, completion controls capture IDs/revisions only.
 * Initialize intent.pending=0 before drawing the board. First click wins. */
unsigned int wena_checklist_contents_render_actions(struct nk_context *context,
    const WenaChecklistBoardContents *contents, const WenaCard *card,
    int board_default, WenaChecklistCompletionIntent *intent);
unsigned int wena_checklist_contents_render_controls(struct nk_context *context,
    const WenaChecklistBoardContents *contents, const WenaCard *card, int board_default,
    WenaChecklistCompletionIntent *intent, WenaCardSectionControl *sections);
/* Optional shared form captures edits for submission after rendering. */
unsigned int wena_checklist_contents_render_editable(struct nk_context *context,
    const WenaChecklistBoardContents *contents, const WenaCard *card, int board_default,
    WenaChecklistCompletionIntent *intent, WenaCardSectionControl *sections,
    WenaChecklistInlineEdit *edit);
#endif
