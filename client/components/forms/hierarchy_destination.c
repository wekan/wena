#include "hierarchy_destination.h"
#include "../../../imports/ui/page_contract.h"
#include <nuklear.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
static int destination_layout_valid(const WenaBoardLayout *layout)
{
    return layout&&layout->board&&!layout->board->archived&&
        wena_model_identifier_valid(layout->board->id)&&
        layout->list_count<=(size_t)INT_MAX&&layout->swimlane_count<=(size_t)INT_MAX&&
        (!layout->list_count||layout->lists)&&(!layout->swimlane_count||layout->swimlanes);
}

const WenaSwimlane *wena_hierarchy_destination_lane(const WenaBoardLayout *layout, const char *id)
{
    size_t i;
    if(!destination_layout_valid(layout)||!wena_model_identifier_valid(id))return NULL;
    for (i = 0; i < layout->swimlane_count; ++i)
        if (!layout->swimlanes[i].archived && !strcmp(layout->swimlanes[i].id, id) &&
            !strcmp(layout->swimlanes[i].board_id, layout->board->id))
            return &layout->swimlanes[i];
    return NULL;
}

const WenaList *wena_hierarchy_destination_list(const WenaBoardLayout *layout, const char *id,
    const char *lane_id)
{
    size_t i;
    if(!destination_layout_valid(layout)||!wena_model_identifier_valid(id))return NULL;
    if(!wena_model_identifier_valid(lane_id))return NULL;
    for (i = 0; i < layout->list_count; ++i) {
        const WenaList *list;
        list = &layout->lists[i];
        if (!list->archived && !strcmp(list->id, id) &&
            !strcmp(list->board_id, layout->board->id) &&
            (list->swimlane_id[0] == '\0' || !strcmp(list->swimlane_id, lane_id))) return list;
    }
    return NULL;
}

typedef struct MoveOptions {
    const WenaBoardLayout *layout;
    const char *lane_id;
    int lists;
    char label[WENA_TITLE_CAPACITY + WENA_ID_CAPACITY + 8];
} MoveOptions;

static const char *option_id(MoveOptions *options, int selected, const char **title)
{
    size_t i;
    int index;
    index = 0;
    if (options->lists) {
        for (i = 0; i < options->layout->list_count; ++i) {
            const WenaList *list = &options->layout->lists[i];
            if (wena_hierarchy_destination_list(options->layout, list->id, options->lane_id) != list) continue;
            if (index++ == selected) { *title = list->title; return list->id; }
        }
    } else {
        for (i = 0; i < options->layout->swimlane_count; ++i) {
            const WenaSwimlane *lane = &options->layout->swimlanes[i];
            if (wena_hierarchy_destination_lane(options->layout, lane->id) != lane) continue;
            if (index++ == selected) { *title = lane->title; return lane->id; }
        }
    }
    *title = wena_ui_text(WENA_UI_TEXT_UNKNOWN);
    return NULL;
}

static void option_label(void *data, int index, const char **text)
{
    MoveOptions *options;
    const char *title, *id;
    options = (MoveOptions *)data;
    id = option_id(options, index, &title);
    if (id == NULL) { *text = wena_ui_text(WENA_UI_TEXT_UNKNOWN); return; }
    else sprintf(options->label, "%s [%s]", title, id);
    *text = options->label;
}

static void render_selector(struct nk_context *context, MoveOptions *options,
    char *target)
{
    int count, selected, result;
    const char *id, *title;
    selected = -1; count = 0;
    while ((id = option_id(options, count, &title)) != NULL) {
        if (!strcmp(id, target)) selected = count;
        ++count;
    }
    if (count == 0) { nk_label_wrap(context, wena_ui_text(WENA_UI_TEXT_NO_ITEMS)); return; }
    /* An invalidated selection must be explicitly repaired before Save. */
    result = nk_combo_callback(context, option_label, options, selected,
                                count, 24, nk_vec2(280, 220));
    if (result >= 0 && result < count && result != selected) {
        id = option_id(options, result, &title);
        if (id != NULL) strcpy(target, id);
    }
}

int wena_hierarchy_destination_render(struct nk_context *context,const WenaBoardLayout *layout,
    WenaId list,WenaId lane)
{
    MoveOptions options;
    if(!context||!destination_layout_valid(layout)||!list||!lane||
        !memchr(list,0,WENA_ID_CAPACITY)||!memchr(lane,0,WENA_ID_CAPACITY))return 0;
    options.layout = layout; options.lane_id = lane;
    options.lists = 0;
    nk_layout_row_dynamic(context, 24, 1);
    nk_label(context, wena_ui_text(WENA_UI_TEXT_SWIMLANE), NK_TEXT_LEFT);
    nk_layout_row_dynamic(context, 28, 1);
    render_selector(context, &options, lane);
    options.lists = 1;
    nk_layout_row_dynamic(context, 24, 1);
    nk_label(context, wena_ui_text(WENA_UI_TEXT_LIST), NK_TEXT_LEFT);
    nk_layout_row_dynamic(context, 28, 1);
    render_selector(context, &options, list);
    return wena_hierarchy_destination_lane(layout,lane)!=NULL&&
        wena_hierarchy_destination_list(layout,list,lane)!=NULL;
}

