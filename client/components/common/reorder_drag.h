#ifndef WENA_REORDER_DRAG_H
#define WENA_REORDER_DRAG_H
#include "../../../models/model.h"
#include <stddef.h>
struct nk_context;
#define WENA_REORDER_SCOPE_CAPACITY 224
typedef struct WenaReorderDrag {
    int active, pending, source_seen, moved;
    char scope[WENA_REORDER_SCOPE_CAPACITY];
    WenaId source_id, target_id;
    unsigned long revision;
    size_t source_position, target_position;
    float start_x, start_y;
} WenaReorderDrag;
void wena_reorder_drag_cancel(WenaReorderDrag *state);
/* Begin/end once around all participating rows, not once per list. The owner
 * consumes pending after end and resolves exact IDs against its snapshot.
 * Scope and revision identify one ordered sibling collection. */
void wena_reorder_drag_begin(struct nk_context *context,WenaReorderDrag *state);
int wena_reorder_drag_handle(struct nk_context *context,WenaReorderDrag *state,
    const char *scope,unsigned long revision,const char *id,size_t position,
    const char *label,int enabled);
void wena_reorder_drag_end(struct nk_context *context,WenaReorderDrag *state);
#endif
