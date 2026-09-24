#include "hierarchy_drag.h"
#include "../../imports/ui/page_contract.h"
#include <nuklear.h>
#include <stdio.h>
#include <string.h>
void wena_hierarchy_drag_init(WenaHierarchyDrag *state,WenaHierarchyMoveLoad load,
    WenaHierarchyMoveApply apply,void *context)
{
    if (!state) return;
    memset(state,0,sizeof(*state));
    wena_hierarchy_move_init(&state->move,load,apply,context);
}
void wena_hierarchy_drag_cancel(WenaHierarchyDrag *state)
{
    if (!state) return;
    wena_reorder_drag_cancel(&state->gesture);
    wena_hierarchy_move_close(&state->move);
}
void wena_hierarchy_drag_begin(struct nk_context *context,WenaHierarchyDrag *state,
    const WenaBoardLayout *layout)
{
    if (!state) return;
    if (state->move.visible && !wena_hierarchy_move_current(&state->move,layout))
        wena_hierarchy_drag_cancel(state);
    wena_reorder_drag_begin(context,&state->gesture);
    if (!state->gesture.active) wena_hierarchy_move_close(&state->move);
}
void wena_hierarchy_drag_handle(struct nk_context *context,WenaHierarchyDrag *state,
    const WenaBoardLayout *layout,WenaHierarchyKind kind,const char *id,size_t position,int enabled)
{
    char scope[WENA_ID_CAPACITY+16];
    unsigned long version;
    if (!state || !layout || !layout->board ||
        !wena_model_identifier_valid(layout->board->id) ||
        (kind!=WENA_HIERARCHY_LIST && kind!=WENA_HIERARCHY_SWIMLANE)) return;
    sprintf(scope,"%s/%s",kind==WENA_HIERARCHY_LIST?"lists":"swimlanes",layout->board->id);
    version=state->move.visible ? state->move.version : 1UL;
    nk_layout_row_dynamic(context,24,1);
    if (wena_reorder_drag_handle(context,&state->gesture,scope,version,id,position,
        wena_ui_control_text(kind==WENA_HIERARCHY_LIST?WENA_UI_MOVE_LIST_TO:WENA_UI_MOVE_SWIMLANE_TO),
        enabled && !state->error && !layout->board->archived && state->move.load && state->move.apply))
        state->kind=kind;
}
void wena_hierarchy_drag_end(struct nk_context *context,WenaHierarchyDrag *state)
{
    if (!state) return;
    wena_reorder_drag_end(context,&state->gesture);
    if (!state->gesture.active && !state->gesture.pending) wena_hierarchy_move_close(&state->move);
}
int wena_hierarchy_drag_process(WenaHierarchyDrag *state,const WenaBoardLayout *layout)
{
    size_t target;
    int valid;
    if (!state || state->error) return 0;
    if (state->gesture.active && !state->move.visible) {
        if (!wena_hierarchy_move_open(&state->move,layout,state->kind,state->gesture.source_id) ||
            state->move.source_position!=(unsigned long)state->gesture.source_position) {
            wena_hierarchy_drag_cancel(state);state->error=1;return -1;
        }
        state->gesture.revision=state->move.version;
    }
    if (!state->gesture.pending) return 0;
    state->gesture.pending=0;
    target=state->gesture.target_position;
    valid=wena_hierarchy_move_current(&state->move,layout) && target<state->move.count &&
        !strcmp(state->move.target_id,state->gesture.source_id) &&
        !strcmp(state->gesture.target_scope,state->gesture.scope) &&
        !strcmp(state->move.order[target],state->gesture.target_id) &&
        state->move.version==state->gesture.revision && state->move.apply &&
        state->move.apply(state->move.context,state->move.board_id,state->move.kind,
            state->move.target_id,state->move.version,(unsigned long)target);
    wena_hierarchy_drag_cancel(state);
    state->error=!valid;
    return valid?1:-1;
}
