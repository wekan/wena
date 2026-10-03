#include "reorder_drag.h"
#include "../../platform/nuklear_options.h"
#include <nuklear.h>
#include <string.h>

void wena_reorder_drag_cancel(WenaReorderDrag *state)
{
    if (state) memset(state,0,sizeof(*state));
}
void wena_reorder_drag_begin(struct nk_context *context,WenaReorderDrag *state)
{
    float dx,dy;
    if (!state) return;
    state->pending=0;
    state->source_seen=0;
    if (!context || nk_input_is_key_pressed(&context->input,NK_KEY_TEXT_RESET_MODE) ||
        (state->active && !nk_input_is_mouse_down(&context->input,NK_BUTTON_LEFT) &&
         !nk_input_is_mouse_released(&context->input,NK_BUTTON_LEFT)))
        wena_reorder_drag_cancel(state);
    if (context && state->active) {
        dx=context->input.mouse.pos.x-state->start_x;
        dy=context->input.mouse.pos.y-state->start_y;
        if (dx*dx+dy*dy>=49.0f) state->moved=1;
    }
}
/* Shared by a labelled handle row and an area handle: the gesture starts on
 * a press over the source and records a release over another sibling. */
static int handle_core(struct nk_context *context,WenaReorderDrag *state,
    const char *scope,unsigned long revision,const char *id,size_t position,
    int valid,int hovered)
{
    int started;
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
        if (hovered && state->moved && !state->pending && !strcmp(scope,state->scope) &&
            revision==state->revision && strcmp(id,state->source_id) &&
            nk_input_is_mouse_released(&context->input,NK_BUTTON_LEFT)) {
            strcpy(state->target_scope,scope);strcpy(state->target_id,id);state->target_position=position;state->pending=1;
        }
    }
    return started;
}
static int handle_valid(struct nk_context *context,const WenaReorderDrag *state,
    const char *scope,unsigned long revision,const char *id,int enabled)
{
    return enabled && scope && scope[0] && strlen(scope)<sizeof(state->scope) &&
        revision>0 && !nk_input_is_key_pressed(&context->input,NK_KEY_TEXT_RESET_MODE) &&
        wena_model_identifier_valid(id) && nk_window_has_focus(context);
}
int wena_reorder_drag_handle(struct nk_context *context,WenaReorderDrag *state,
    const char *scope,unsigned long revision,const char *id,size_t position,
    const char *label,int enabled)
{
    int valid,hovered,started;
    if (!context || !state || !label) return 0;
    valid=handle_valid(context,state,scope,revision,id,enabled);
    hovered=valid && nk_widget_is_hovered(context) &&
        nk_input_is_mouse_hovering_rect(&context->input,context->current->layout->clip);
    started=handle_core(context,state,scope,revision,id,position,valid,hovered);
    if (valid) (void)nk_button_label(context,label);
    else nk_label(context,label,NK_TEXT_LEFT);
    return started;
}
int wena_reorder_drag_area(struct nk_context *context,WenaReorderDrag *state,
    const char *scope,unsigned long revision,const char *id,size_t position,
    const struct nk_rect *area,int enabled,int *clicked)
{
    int valid,hovered,started,was_source;
    if (clicked) *clicked=0;
    if (!context || !state || !area || !context->current) return 0;
    valid=handle_valid(context,state,scope,revision,id,enabled);
    hovered=nk_input_is_mouse_hovering_rect(&context->input,*area) &&
        nk_input_is_mouse_hovering_rect(&context->input,context->current->layout->clip);
    was_source=state->active && !state->moved && scope && !strcmp(state->scope,scope) &&
        id && !strcmp(state->source_id,id);
    started=handle_core(context,state,scope,revision,id,position,valid,valid && hovered);
    /* Pressed and released here without moving: a click, as in WeKan, where
     * the same minicard or header is both clicked and dragged. A source that
     * cannot be dragged still clicks. */
    if (clicked && hovered && nk_input_is_mouse_released(&context->input,NK_BUTTON_LEFT) &&
        (was_source || (!valid && !state->active &&
         nk_input_has_mouse_click_in_rect(&context->input,NK_BUTTON_LEFT,*area))))
        *clicked=1;
    return started;
}
int wena_reorder_drag_drop(struct nk_context *context,WenaReorderDrag *state,
    const char *scope,const char *id,const char *label,int enabled)
{
    int dropped,valid;
    if (!context || !state || !label) return 0;
    valid=enabled && scope && scope[0] && strlen(scope)<sizeof(state->target_scope) &&
        wena_model_identifier_valid(id) && nk_window_has_focus(context);
    dropped=valid && state->active && state->moved && !state->pending &&
        !nk_input_is_key_pressed(&context->input,NK_KEY_TEXT_RESET_MODE) &&
        nk_widget_is_hovered(context) &&
        nk_input_is_mouse_hovering_rect(&context->input,context->current->layout->clip) &&
        nk_input_is_mouse_released(&context->input,NK_BUTTON_LEFT);
    if (dropped) {
        strcpy(state->target_scope,scope);strcpy(state->target_id,id);
        state->target_position=0;state->pending=1;
    }
    if (valid) (void)nk_button_label(context,label);
    else nk_label(context,label,NK_TEXT_LEFT);
    return dropped;
}
int wena_reorder_drag_drop_area(struct nk_context *context,WenaReorderDrag *state,
    const char *scope,const char *id,const struct nk_rect *area,int enabled)
{
    int dropped,valid;
    if (!context || !state || !area || !context->current) return 0;
    valid=enabled && scope && scope[0] && strlen(scope)<sizeof(state->target_scope) &&
        wena_model_identifier_valid(id) && nk_window_has_focus(context);
    dropped=valid && state->active && state->moved && !state->pending &&
        !nk_input_is_key_pressed(&context->input,NK_KEY_TEXT_RESET_MODE) &&
        nk_input_is_mouse_hovering_rect(&context->input,*area) &&
        nk_input_is_mouse_hovering_rect(&context->input,context->current->layout->clip) &&
        nk_input_is_mouse_released(&context->input,NK_BUTTON_LEFT);
    if (dropped) {
        strcpy(state->target_scope,scope);strcpy(state->target_id,id);
        state->target_position=0;state->pending=1;
    }
    return dropped;
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
