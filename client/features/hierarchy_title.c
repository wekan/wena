#include "labels/component.h"
#include "hierarchy_title.h"
#include "../../imports/ui/page_contract.h"

#include <nuklear.h>
#include <string.h>
#include <stdio.h>

static int selected(const WenaBoardLayout *layout, WenaHierarchyKind kind,
                      const char *board, const char *target)
{
    size_t index;
    if (!layout || !layout->board || layout->board->archived || !board ||
        !target || !target[0] || strcmp(layout->board->id, board) != 0 ||
        (layout->list_count && !layout->lists) ||
        (layout->swimlane_count && !layout->swimlanes)) return 0;
    if (kind == WENA_HIERARCHY_BOARD) return strcmp(board, target) == 0;
    if (kind == WENA_HIERARCHY_LIST) {
        for (index = 0; index < layout->list_count; ++index) {
            if (!layout->lists[index].archived &&
                strcmp(layout->lists[index].id, target) == 0 &&
                strcmp(layout->lists[index].board_id, board) == 0) return 1;
        }
    } else if (kind == WENA_HIERARCHY_SWIMLANE) {
        for (index = 0; index < layout->swimlane_count; ++index) {
            if (!layout->swimlanes[index].archived &&
                strcmp(layout->swimlanes[index].id, target) == 0 &&
                strcmp(layout->swimlanes[index].board_id, board) == 0) return 1;
        }
    }
    return 0;
}

void wena_hierarchy_title_init(WenaHierarchyTitleState *state)
{
    if (state) memset(state, 0, sizeof(*state));
}

void wena_hierarchy_title_close(WenaHierarchyTitleState *state)
{
    if (!state) return;
    state->visible = 0;
    state->requested_action = 0u;
    state->creating = 0;
    state->editing_color = 0;
    state->editing_wip=0;state->wip_length=0;state->wip_value[0]=0;
    memset(&state->color_input,0,sizeof(state->color_input));
    state->error = 0;
    state->title_length = 0;
    state->board_id[0] = '\0';
    state->target_id[0] = '\0';
    state->original_title[0] = '\0';
    state->title_input[0] = '\0';
    state->title_version = 0;
}

void wena_hierarchy_title_set_adapter(WenaHierarchyTitleState *state,
    WenaHierarchyLoadTitle load, WenaHierarchySaveTitle save, void *context)
{
    if (!state) return;
    wena_hierarchy_title_close(state);
    state->load_title = load;
    state->save_title = save;
    state->context = context;
    state->create_title = NULL;
    state->archive = NULL;
    state->load_color = NULL;state->save_color = NULL;
    state->load_wip=NULL;state->save_wip=NULL;
}

void wena_hierarchy_title_set_create_adapter(WenaHierarchyTitleState *state,
    WenaHierarchyCreateTitle create)
{
    if (!state) return;
    wena_hierarchy_title_close(state);
    state->create_title = create;
}

void wena_hierarchy_title_set_archive_adapter(WenaHierarchyTitleState *state,
    WenaHierarchyArchive archive)
{
    if(!state)return;
    wena_hierarchy_title_close(state);state->archive=archive;
}

void wena_hierarchy_title_set_color_adapters(WenaHierarchyTitleState *state,
    WenaHierarchyLoadTitle load,WenaHierarchySaveTitle save)
{
    if(!state)return;
    wena_hierarchy_title_close(state);state->load_color=load;state->save_color=save;
}

void wena_hierarchy_title_set_wip_adapters(WenaHierarchyTitleState *state,
    WenaHierarchyLoadWip load,WenaHierarchySaveWip save)
{
    if(!state)return;
    wena_hierarchy_title_close(state);state->load_wip=load;state->save_wip=save;
}
static int render_wip(struct nk_context *context,WenaHierarchyTitleState *state,float width,float height)
{
    int save,cancel,enabled,soft,action,i,valid;size_t value;char count[32];
    save=cancel=0;action=-1;enabled=state->wip_limit.enabled;soft=state->wip_limit.soft;
    if(nk_begin_titled(context,"List WIP",wena_ui_text(WENA_UI_TEXT_EDIT_WIP_LIMIT),
        nk_rect(width*0.15f,0,width*0.7f,height<360?height:360),NK_WINDOW_BORDER)){
        if(wena_title_input_keys(context,0u)&WENA_TITLE_INPUT_CANCEL){nk_end(context);wena_hierarchy_title_close(state);return 1;}
        nk_layout_row_dynamic(context,28,1);nk_label_wrap(context,state->target_id);
        sprintf(count,"%lu / %lu",(unsigned long)state->wip_count,(unsigned long)state->wip_limit.value);
        nk_label(context,count,NK_TEXT_LEFT);
        if(nk_checkbox_label(context,wena_ui_text(WENA_UI_TEXT_ENABLE_WIP_LIMIT),&enabled))action=WENA_WIP_TOGGLE_ENABLED;
        if(nk_checkbox_label(context,wena_ui_text(WENA_UI_TEXT_SOFT_WIP_LIMIT),&soft))action=WENA_WIP_TOGGLE_SOFT;
        if(state->wip_length<0||state->wip_length>(int)sizeof(state->wip_value)){state->wip_length=0;state->error=1;}
        nk_edit_string(context,NK_EDIT_FIELD,state->wip_value,&state->wip_length,(int)sizeof(state->wip_value),nk_filter_default);
        nk_layout_row_dynamic(context,28,2);
        save=nk_button_label(context,wena_ui_control_text(WENA_UI_SAVE));cancel=nk_button_label(context,wena_ui_control_text(WENA_UI_CANCEL));
        if(state->error){nk_layout_row_dynamic(context,48,1);nk_label_wrap(context,wena_ui_text(WENA_UI_TEXT_OPERATION_FAILED));}
    }
    nk_end(context);
    if(cancel){wena_hierarchy_title_close(state);return 1;}
    value=0;valid=1;
    if(save){action=WENA_WIP_APPLY_VALUE;valid=state->wip_length>0&&state->wip_length<=2;
        for(i=0;valid&&i<state->wip_length;++i){if(state->wip_value[i]<'0'||state->wip_value[i]>'9')valid=0;else value=value*10+(size_t)(state->wip_value[i]-'0');}
        if(!value)valid=0;}
    if(action>=0){if(valid&&state->save_wip&&state->save_wip(state->context,state->board_id,state->target_id,
        state->title_version,(WenaWipEdit)action,value))wena_hierarchy_title_close(state);else state->error=1;}
    return 1;
}

int wena_hierarchy_title_open_create(WenaHierarchyTitleState *state,
    const WenaBoardLayout *layout, WenaHierarchyKind kind)
{
    if (!state) return 0;
    wena_hierarchy_title_close(state);
    if (!layout || !layout->board || !state->create_title ||
        (kind != WENA_HIERARCHY_LIST && kind != WENA_HIERARCHY_SWIMLANE) ||
        !selected(layout, WENA_HIERARCHY_BOARD, layout->board->id, layout->board->id) ||
        !wena_model_set_required(state->board_id, sizeof(state->board_id), layout->board->id))
        return 0;
    state->kind = kind;
    state->creating = 1;
    state->visible = 1;
    return 1;
}

int wena_hierarchy_title_open(WenaHierarchyTitleState *state,
    const WenaBoardLayout *layout, WenaHierarchyKind kind, const char *target)
{
    if (!state) return 0;
    wena_hierarchy_title_close(state);
    if (!layout || !layout->board || !state->load_title || !state->save_title ||
        !selected(layout, kind, layout->board->id, target) ||
        !wena_model_set_required(state->board_id, sizeof(state->board_id),
                                  layout->board->id) ||
        !wena_model_set_required(state->target_id, sizeof(state->target_id), target))
        return 0;
    state->kind = kind;
    memset(state->title_input, 0, sizeof(state->title_input));
    if (!state->load_title(state->context, state->board_id, kind, state->target_id,
            state->title_input, WENA_CARD_DETAILS_TITLE_CAPACITY,
            &state->title_version) || state->title_version == 0 ||
        memchr(state->title_input, '\0', WENA_CARD_DETAILS_TITLE_CAPACITY) == NULL ||
        !wena_card_details_title_valid(state->title_input,
                                        strlen(state->title_input))) {
        wena_hierarchy_title_close(state);
        return 0;
    }
    state->title_length = (int)strlen(state->title_input);
    strcpy(state->original_title,state->title_input);
    state->visible = 1;
    return 1;
}

static int render_color(struct nk_context *context,WenaHierarchyTitleState *state,float width,float height)
{
    int save,cancel;const char *color;float panel_height;
    panel_height=height<440.0f?height:440.0f;save=cancel=0;
    if(nk_begin_titled(context,"Hierarchy color",wena_ui_text(WENA_UI_TEXT_SELECT_COLOR),
        nk_rect(width*0.15f,0,width*0.7f,panel_height),NK_WINDOW_BORDER)){
        if(wena_title_input_keys(context,0u)&WENA_TITLE_INPUT_CANCEL){
            nk_end(context);wena_hierarchy_title_close(state);return 1;}
        nk_layout_row_dynamic(context,28,1);
        nk_label_wrap(context,state->target_id);
        wena_color_input_render(context,&state->color_input,state->original_title,wena_label_badge_render);
        nk_layout_row_dynamic(context,28,1);
        if(nk_button_label(context,wena_ui_text(WENA_UI_TEXT_DEFAULT)))
            (void)wena_color_input_set(&state->color_input,"");
        nk_layout_row_dynamic(context,28,2);
        save=nk_button_label(context,wena_ui_control_text(WENA_UI_SAVE));
        cancel=nk_button_label(context,wena_ui_control_text(WENA_UI_CANCEL));
        if(state->error){nk_layout_row_dynamic(context,48,1);nk_label_wrap(context,wena_ui_text(WENA_UI_TEXT_OPERATION_FAILED));}
    }
    nk_end(context);
    if(cancel)wena_hierarchy_title_close(state);
    else if(save){color=wena_color_input_value(&state->color_input);
        if(color&&state->save_color&&state->save_color(state->context,state->board_id,
            state->kind,state->target_id,state->title_version,color))wena_hierarchy_title_close(state);
        else state->error=1;}
    return 1;
}

int wena_hierarchy_title_render(struct nk_context *context,
    WenaHierarchyTitleState *state, const WenaBoardLayout *layout,
    float width, float height)
{
    int cancel;
    int archive;
    int color_requested;
    int wip_requested;
    int save;
    unsigned int edit_keys;
    if (state) state->requested_action = 0u;
    if (!state || !state->visible || !context || width <= 0.0f || height <= 0.0f)
        return 0;
    if (!selected(layout, state->creating ? WENA_HIERARCHY_BOARD : state->kind,
        state->board_id, state->creating ? state->board_id : state->target_id) ||
        (state->creating && state->kind != WENA_HIERARCHY_LIST &&
         state->kind != WENA_HIERARCHY_SWIMLANE)) {
        wena_hierarchy_title_close(state);
        return 0;
    }
    if(state->editing_wip)return render_wip(context,state,width,height);
    if(state->editing_color)return render_color(context,state,width,height);
    save = cancel = archive = color_requested = wip_requested = 0;
    if (nk_begin_titled(context, "Edit hierarchy title",
        wena_ui_control_text(state->creating ?
            (state->kind == WENA_HIERARCHY_LIST ? WENA_UI_ADD_LIST :
             WENA_UI_ADD_SWIMLANE) : WENA_UI_EDIT_TITLE),
        nk_rect(width * 0.2f, height * 0.2f, width * 0.6f, 210.0f + (state->load_wip && state->save_wip && !state->creating && state->kind==WENA_HIERARCHY_LIST ? 40.0f : 0.0f) + (state->archive && !state->creating && state->kind==WENA_HIERARCHY_LIST ? 40.0f : 0.0f) + (state->load_color && state->save_color && !state->creating && state->kind!=WENA_HIERARCHY_BOARD ? 40.0f : 0.0f)),
        NK_WINDOW_BORDER)) {
        nk_layout_row_dynamic(context, 24.0f, 1);
        nk_label(context, wena_ui_control_text(state->creating ?
            (state->kind == WENA_HIERARCHY_LIST ? WENA_UI_ADD_LIST :
             WENA_UI_ADD_SWIMLANE) : WENA_UI_EDIT_TITLE), NK_TEXT_LEFT);
        nk_layout_row_dynamic(context, 32.0f, 1);
        edit_keys = wena_title_input_keys(context,
            nk_edit_string(context, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER,
                state->title_input, &state->title_length,
                (int)sizeof(state->title_input), nk_filter_default));
        nk_layout_row_dynamic(context, 28.0f, 2);
        save = nk_button_label(context, wena_ui_control_text(WENA_UI_SAVE));
        cancel = nk_button_label(context, wena_ui_control_text(WENA_UI_CANCEL));
        save = save || (edit_keys & WENA_TITLE_INPUT_COMMIT) != 0u;
        cancel = cancel || (edit_keys & WENA_TITLE_INPUT_CANCEL) != 0u;
        if (!state->creating && (state->kind == WENA_HIERARCHY_LIST ||
            state->kind == WENA_HIERARCHY_SWIMLANE)) {
            nk_layout_row_dynamic(context, 28.0f, 1);
            if (nk_button_label(context, wena_ui_control_text(
                state->kind == WENA_HIERARCHY_LIST ? WENA_UI_MOVE_LIST_TO :
                WENA_UI_MOVE_SWIMLANE_TO))) state->requested_action = WENA_HIERARCHY_TITLE_MOVE;
        }
        if(!state->creating&&state->kind!=WENA_HIERARCHY_BOARD&&state->load_color&&state->save_color){
            nk_layout_row_dynamic(context,28.0f,1);
            color_requested=nk_button_label(context,wena_ui_text(WENA_UI_TEXT_SELECT_COLOR));
        }
        if(!state->creating&&state->kind==WENA_HIERARCHY_LIST&&state->load_wip&&state->save_wip){
            nk_layout_row_dynamic(context,28,1);wip_requested=nk_button_label(context,wena_ui_text(WENA_UI_TEXT_EDIT_WIP_LIMIT));
        }
        if(!state->creating&&state->kind==WENA_HIERARCHY_LIST&&state->archive){
            nk_layout_row_dynamic(context,28.0f,1);
            archive=nk_button_label(context,wena_ui_control_text(WENA_UI_ARCHIVE_LIST));
        }
        if (state->error) {
            nk_layout_row_dynamic(context, 24.0f, 1);
            nk_label_wrap(context, wena_ui_text(WENA_UI_TEXT_OPERATION_FAILED));
        }
    }
    nk_end(context);
    if (cancel) wena_hierarchy_title_close(state);
    else if(wip_requested){
        WenaWipLimit limit;size_t count;unsigned long version;
        memset(&limit,0,sizeof(limit));count=0;version=0;
        if(state->load_wip(state->context,state->board_id,state->target_id,&limit,&count,&version)&&
            wena_wip_limit_valid(&limit)&&limit.value<=2147483647&&count<=2147483647&&version&&version<=WENA_VERSION_READ_MAX){
            state->wip_limit=limit;state->wip_count=count;state->title_version=version;
            sprintf(state->wip_value,"%lu",(unsigned long)limit.value);state->wip_length=(int)strlen(state->wip_value);
            state->editing_wip=1;state->error=0;
        }else state->error=1;
    }
    else if(color_requested){
        char color[WENA_COLOR_CAPACITY];unsigned long version;
        memset(color,0,sizeof(color));version=0;
        if(state->load_color(state->context,state->board_id,state->kind,state->target_id,
            color,sizeof(color),&version)&&version&&version<=WENA_VERSION_READ_MAX&&
            memchr(color,0,sizeof(color))&&wena_color_input_set(&state->color_input,color)){
            state->editing_color=1;state->title_version=version;state->error=0;
        }else state->error=1;
    }
    else if(archive){
        if(state->archive(state->context,state->board_id,state->target_id,state->title_version))
            wena_hierarchy_title_close(state);
        else state->error=1;
    }
    else if (save) {
        if (state->title_length >= 0 &&
            wena_card_details_title_valid(state->title_input,
                                            (size_t)state->title_length)) {
            state->title_input[state->title_length] = '\0';
            if (state->creating ?
                (state->create_title && state->create_title(state->context,
                    state->board_id, state->kind, state->title_input)) :
                (state->save_title && state->save_title(state->context,
                    state->board_id, state->kind, state->target_id,
                    state->title_version, state->title_input))) {
                wena_hierarchy_title_close(state);
                return 1;
            }
        }
        state->error = 1;
    }
    return 1;
}
