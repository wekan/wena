#ifndef WENA_CHECKLIST_CONTENTS_COMPONENT_H
#define WENA_CHECKLIST_CONTENTS_COMPONENT_H
#include "../../features/checklists/summary.h"
struct nk_context;
/* Read-only preview from an immutable validated snapshot. Explicit title clicks
 * return OPEN_CHECKLISTS for the caller's exact card. No SQL or writes here. */
unsigned int wena_checklist_contents_render(struct nk_context *context,
    const WenaChecklistBoardContents *contents, const WenaCard *card,
    int board_default);
#endif
