#ifndef WENA_CHECKLIST_BADGES_H
#define WENA_CHECKLIST_BADGES_H
#include "summary.h"
struct nk_context;
/* Compact derived counts only. Returns OPEN_CHECKLISTS for the explicit button. */
unsigned int wena_checklist_badges_render(struct nk_context *context,
    const WenaChecklistBoardSummary *summary, const WenaCard *card);
#endif
