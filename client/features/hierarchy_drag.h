#ifndef WENA_HIERARCHY_DRAG_H
#define WENA_HIERARCHY_DRAG_H
#include "hierarchy_move.h"
#include "../components/common/reorder_drag.h"
typedef struct WenaHierarchyDrag {
    WenaReorderDrag gesture;
    WenaHierarchyMoveState move;
    WenaHierarchyKind kind;
    int error;
} WenaHierarchyDrag;
void wena_hierarchy_drag_init(WenaHierarchyDrag*,WenaHierarchyMoveLoad,WenaHierarchyMoveApply,void*);
void wena_hierarchy_drag_cancel(WenaHierarchyDrag*);
void wena_hierarchy_drag_begin(struct nk_context*,WenaHierarchyDrag*,const WenaBoardLayout*);
void wena_hierarchy_drag_handle(struct nk_context*,WenaHierarchyDrag*,const WenaBoardLayout*,
    WenaHierarchyKind,const char *id,size_t position,int enabled);
void wena_hierarchy_drag_end(struct nk_context*,WenaHierarchyDrag*);
/* Outside drawing: capture authoritative revision once on press; apply once on
 * release using the same snapshot validation and guarded adapter as Move. */
int wena_hierarchy_drag_process(WenaHierarchyDrag*,const WenaBoardLayout*);
#endif
