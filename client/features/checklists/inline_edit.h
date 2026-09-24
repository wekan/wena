#ifndef WENA_CHECKLIST_INLINE_EDIT_H
#define WENA_CHECKLIST_INLINE_EDIT_H
#include "summary.h"
#include "../../../models/checklist_item_titles.h"
#include "../checklist_store.h"
#include "../../components/forms/input_limits.h"
struct nk_context;
typedef struct WenaChecklistInlineEdit {
    WenaChecklistAction action;
    WenaId board_id,card_id,checklist_id,item_id;
    unsigned long card_version,checklist_version,item_version;
    char input[WENA_NATIVE_EDIT_CAPACITY(WENA_CHECKLIST_BATCH_MAX_BYTES + 1u)];
    int length,error,pending;
} WenaChecklistInlineEdit;
void wena_checklist_inline_cancel(WenaChecklistInlineEdit *edit);
int wena_checklist_inline_begin(WenaChecklistInlineEdit *edit,
    const WenaChecklistBoardContents *contents,const WenaCard *card,
    const WenaChecklistContents *list,size_t item,WenaChecklistAction action);
/* Reuse one form in any host view. Render only the matching selected checklist.
 * A failed save retains the draft; explicit Save can retry after validation. */
void wena_checklist_inline_render(struct nk_context *context,WenaChecklistInlineEdit *edit);
/* Cancel drafts whose exact card/checklist/item disappeared or was archived. */
void wena_checklist_inline_sync(WenaChecklistInlineEdit *edit,
    const WenaChecklistBoardContents *contents, int board_default);
#endif
