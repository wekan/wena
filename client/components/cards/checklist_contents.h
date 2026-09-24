#ifndef WENA_CHECKLIST_CONTENTS_COMPONENT_H
#define WENA_CHECKLIST_CONTENTS_COMPONENT_H
#include "../../features/checklists/summary.h"
#include "../../features/checklist_store.h"
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
#endif
