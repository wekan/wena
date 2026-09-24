#ifndef WENA_HIERARCHY_MOVE_H
#define WENA_HIERARCHY_MOVE_H
#include "hierarchy_title.h"

#define WENA_HIERARCHY_MOVE_CAPACITY 128u

typedef int (*WenaHierarchyMoveLoad)(void *context, const char *board_id,
    WenaHierarchyKind kind, const char *target_id, unsigned long *version,
    unsigned long *position);
typedef int (*WenaHierarchyMoveApply)(void *context, const char *board_id,
    WenaHierarchyKind kind, const char *target_id, unsigned long expected_version,
    unsigned long target_position);

typedef struct WenaHierarchyMoveState {
    int visible;
    int error;
    WenaHierarchyKind kind;
    WenaId board_id;
    WenaId target_id;
    WenaId order[WENA_HIERARCHY_MOVE_CAPACITY];
    size_t count;
    unsigned long version;
    unsigned long source_position;
    int target_position;
    WenaHierarchyMoveLoad load;
    WenaHierarchyMoveApply apply;
    void *context;
} WenaHierarchyMoveState;

void wena_hierarchy_move_init(WenaHierarchyMoveState *state,
    WenaHierarchyMoveLoad load, WenaHierarchyMoveApply apply, void *context);
void wena_hierarchy_move_close(WenaHierarchyMoveState *state);
int wena_hierarchy_move_open(WenaHierarchyMoveState *state,
    const WenaBoardLayout *layout, WenaHierarchyKind kind, const char *target_id);
/* Pure comparison shared by dialog and drag lifecycle; never queries storage. */
int wena_hierarchy_move_current(WenaHierarchyMoveState *state,const WenaBoardLayout *layout);
int wena_hierarchy_move_render(struct nk_context *context,
    WenaHierarchyMoveState *state, const WenaBoardLayout *layout,
    float width, float height);
#endif
