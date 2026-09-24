#include "card_drag.h"
#include "../../imports/ui/page_contract.h"
#include <nuklear.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void wena_card_drag_cancel(WenaCardDrag *state)
{
    if (!state) return;
    free(state->order);state->order=NULL;state->order_count=0;
    wena_reorder_drag_cancel(&state->gesture);
}
void wena_card_drag_begin(struct nk_context *context,WenaCardDrag *state,
    const WenaCard *cards,size_t count,unsigned long source_revision)
{
    if (!state) return;
    if (state->gesture.active && (source_revision!=state->gesture.revision ||
        !wena_card_order_current(state->order,state->order_count,cards,count,
            state->board_id,state->list_id,state->swimlane_id)))
        wena_card_drag_cancel(state);
    wena_reorder_drag_begin(context,&state->gesture);
}
void wena_card_drag_handle(struct nk_context *context,WenaCardDrag *state,
    const WenaCard *cards,size_t count,const WenaCard *card,size_t ordinal,
    unsigned long card_revision,int enabled)
{
    char scope[WENA_REORDER_SCOPE_CAPACITY];
    unsigned long revision;
    if (!context || !state || !card || card->archived ||
        !wena_model_identifier_valid(card->board_id) ||
        !wena_model_identifier_valid(card->list_id) ||
        !wena_model_identifier_valid(card->swimlane_id)) return;
    sprintf(scope,"card:%s/%s/%s",card->board_id,card->list_id,card->swimlane_id);
    revision=state->gesture.active ? state->gesture.revision : card_revision;
    nk_layout_row_dynamic(context,24,1);
    if (wena_reorder_drag_handle(context,&state->gesture,scope,revision,card->id,
        ordinal,wena_ui_text(WENA_UI_TEXT_MOVE_SELECTION),enabled && !state->error && card_revision>0)) {
        strcpy(state->board_id,card->board_id);strcpy(state->list_id,card->list_id);
        strcpy(state->swimlane_id,card->swimlane_id);
        if (!wena_card_order_capture(cards,count,state->board_id,state->list_id,
            state->swimlane_id,&state->order,&state->order_count)) {
            wena_card_drag_cancel(state);state->error=1;
        }
    }
}
void wena_card_drag_end(struct nk_context *context,WenaCardDrag *state)
{
    if (!state) return;
    wena_reorder_drag_end(context,&state->gesture);
    if (!state->gesture.active && !state->gesture.pending) wena_card_drag_cancel(state);
}
int wena_card_drag_apply(WenaCardDrag *state,WenaCardMutation *adapter)
{
    int valid;
    if (!state || !state->gesture.pending) return 0;
    state->gesture.pending=0;
    valid=adapter && state->order && state->gesture.target_position<state->order_count &&
        !strcmp(state->order[state->gesture.target_position].id,state->gesture.target_id) &&
        wena_card_order_current(state->order,state->order_count,adapter->cards,
            adapter->card_count,state->board_id,state->list_id,state->swimlane_id);
    if (valid) valid=wena_card_mutation_reorder(adapter,state->board_id,
        state->gesture.source_id,state->gesture.revision,
        (unsigned long)state->gesture.target_position);
    wena_card_drag_cancel(state);state->error=!valid;
    return valid ? 1 : -1;
}
