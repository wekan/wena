#ifndef WENA_CHECKLIST_DRAG_H
#define WENA_CHECKLIST_DRAG_H
#include "summary.h"
#include "../checklist_store.h"
#include "../../components/common/reorder_drag.h"
typedef struct WenaChecklistDrag {
    WenaReorderDrag gesture;
    WenaChecklistCompletionIntent source;
    WenaChecklistAction action;
    WenaId target_card_id, target_checklist_id;
    unsigned long target_card_version, target_checklist_version;
    int insert_at_position;
    int error;
} WenaChecklistDrag;
/* One handle widget in the host row. All sibling ordinals include hidden rows. */
void wena_checklist_drag_handle(struct nk_context *context,WenaChecklistDrag *state,
    const WenaChecklistBoardContents *contents,const WenaCard *card,
    const WenaChecklistContents *list,size_t position,WenaChecklistAction action,
    int enabled);
/* Whole-checklist destination is another card (list=NULL); item destination is
 * a different checklist. Both use append semantics and current target revisions. */
void wena_checklist_drag_destination(struct nk_context *context,WenaChecklistDrag *state,
    const WenaChecklistBoardContents *contents,const WenaCard *card,
    const WenaChecklistContents *list);
/* Explicit slot before a visible sibling, using its complete-collection ordinal. */
void wena_checklist_drag_destination_at(struct nk_context*,WenaChecklistDrag*,
    const WenaChecklistBoardContents*,const WenaCard*,const WenaChecklistContents*,size_t);
#endif
