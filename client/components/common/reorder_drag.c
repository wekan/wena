#include "reorder_drag.h"
#include <nuklear.h>
#include <string.h>

void wena_reorder_drag_cancel(WenaReorderDrag *state)
{
    if (state) memset(state,0,sizeof(*state));
}
void wena_reorder_drag_begin(struct nk_context *context,WenaReorderDrag *state)
{
    if (!state) return;
    state->pending=0;
    state->source_seen=0;
    if (!context || nk_input_is_key_pressed(&context->input,NK_KEY_TEXT_RESET_MODE) ||
        (state->active && !nk_input_is_mouse_down(&context->input,NK_BUTTON_LEFT) &&
         !nk_input_is_mouse_released(&context->input,NK_BUTTON_LEFT)))
        wena_reorder_drag_cancel(state);
}
int wena_reorder_drag_handle(struct nk_context *context,WenaReorderDrag *state,
    const char *scope,unsigned long revision,const char *id,size_t position,
    const char *label,int enabled)
{
    int valid,hovered,started;
    float dx,dy;
    if (!context || !state || !label) return 0;
    valid=enabled && scope && scope[0] && strlen(scope)<sizeof(state->scope) &&
        revision>0 && !nk_input_is_key_pressed(&context->input,NK_KEY_TEXT_RESET_MODE) &&
        wena_model_identifier_valid(id) && nk_window_has_focus(context);
    hovered=valid && nk_widget_is_hovered(context) &&
        nk_input_is_mouse_hovering_rect(&context->input,context->current->layout->clip);
    started=0;
    if (state->active && scope && !strcmp(state->scope,scope)) {
        if (revision!=state->revision) wena_reorder_drag_cancel(state);
        else if (valid && !strcmp(state->source_id,id)) state->source_seen=1;
    }
    if (hovered && !state->active &&
        nk_input_is_mouse_pressed(&context->input,NK_BUTTON_LEFT)) {
        wena_reorder_drag_cancel(state);
        strcpy(state->scope,scope);strcpy(state->source_id,id);
        state->revision=revision;state->source_position=position;
        state->start_x=context->input.mouse.pos.x;state->start_y=context->input.mouse.pos.y;
        state->active=1;state->source_seen=1;started=1;
    }
    if (state->active) {
        dx=context->input.mouse.pos.x-state->start_x;
        dy=context->input.mouse.pos.y-state->start_y;
        if (dx*dx+dy*dy>=49.0f) state->moved=1;
        if (hovered && state->moved && !state->pending && !strcmp(scope,state->scope) &&
            revision==state->revision && strcmp(id,state->source_id) &&
            nk_input_is_mouse_released(&context->input,NK_BUTTON_LEFT)) {
            strcpy(state->target_id,id);state->target_position=position;state->pending=1;
        }
    }
    if (valid) (void)nk_button_label(context,label);
    else nk_label(context,label,NK_TEXT_LEFT);
    return started;
}
void wena_reorder_drag_end(struct nk_context *context,WenaReorderDrag *state)
{
    if (!state || !state->active) return;
    if (!context || !state->source_seen) {
        wena_reorder_drag_cancel(state);
    } else if (nk_input_is_mouse_released(&context->input,NK_BUTTON_LEFT)) {
        state->active=0;
        if (!state->pending) wena_reorder_drag_cancel(state);
    }
}
