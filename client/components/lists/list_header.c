#include "../common/color_heading.h"
#include "list_header.h"
#include "../common/wekan_look.h"
#include "../../../models/color.h"
#include "../../../imports/ui/page_contract.h"

#include "../../platform/nuklear_options.h"
#include <nuklear.h>
#include <string.h>
#include <stdio.h>

unsigned int wena_list_header_render(struct nk_context *context,
                                     const WenaList *list, size_t active_count)
{
    unsigned int action;WenaWipDecision status,addition;int valid;char count[64];

    if (context == NULL || list == NULL || list->archived) {
        return WENA_LIST_HEADER_NO_ACTION;
    }
    action = WENA_LIST_HEADER_NO_ACTION;
    /* Titles use the full column; actions never consume their text width. */
    nk_layout_row_dynamic(context, strlen(list->title) > 32u ? 96.0f : 28.0f, 1);
    wena_color_heading(context,list->title,list->color,1);
    valid=wena_wip_evaluate(&list->wip_limit,active_count,0,0,&status)&&
        wena_wip_evaluate(&list->wip_limit,active_count,0,1,&addition);
    if(valid&&list->wip_limit.enabled){
        sprintf(count,"%lu / %lu",(unsigned long)active_count,(unsigned long)list->wip_limit.value);
        nk_layout_row_dynamic(context,28.0f,1);
        wena_color_heading(context,count,status.exceeded?"red":status.reached?"orange":"",0);
    }
    nk_layout_row_dynamic(context, 28.0f, 2);
    if(valid&&addition.allowed){
        if(nk_button_label(context,wena_ui_control_text(WENA_UI_ADD_CARD)))action|=WENA_LIST_HEADER_ADD_CARD;
    }else nk_label(context,wena_ui_control_text(WENA_UI_ADD_CARD),NK_TEXT_LEFT);
    if (nk_button_label(context, wena_ui_control_text(WENA_UI_LIST_MENU))) {
        action |= WENA_LIST_HEADER_OPEN_MENU;
    }
    return action;
}

unsigned int wena_list_header_render_wekan(struct nk_context *context,
                                           const WenaList *list, size_t active_count,
                                           int collapsed, int *collapse_clicked,
                                           struct nk_rect *title_area)
{
    unsigned int action;
    WenaWipDecision status, addition;
    int valid, title_color;
    struct nk_rect area;
    struct nk_command_buffer *out;
    unsigned char rgb[3], foreground[3];
    char count[64];
    float title_width;

    if (collapse_clicked) *collapse_clicked = 0;
    if (context == NULL || list == NULL || list->archived) return WENA_LIST_HEADER_NO_ACTION;
    action = WENA_LIST_HEADER_NO_ACTION;
    wena_ui_region("list-header");
    nk_layout_space_begin(context, NK_STATIC, WENA_LIST_HEADER_HEIGHT, 6);
    wena_wekan_space_area(context, WENA_LIST_HEADER_HEIGHT, &area.x, &area.y, &area.w);
    area.h = WENA_LIST_HEADER_HEIGHT;
    out = nk_window_get_canvas(context);
    title_color = WENA_WEKAN_TEXT;
    /* A colored list paints its header in that color, as WeKan does. */
    if (list->color[0] && wena_color_rgb(list->color, rgb) && wena_color_foreground(list->color, foreground)) {
        nk_fill_rect(out, area, 0.0f, nk_rgb(rgb[0], rgb[1], rgb[2]));
        title_color = foreground[0] > 128 ? WENA_WEKAN_BUTTON_TEXT : WENA_WEKAN_TEXT;
    } else wena_wekan_fill(context, area.x, area.y, area.w, area.h, WENA_WEKAN_LIST_HEADER, 0.0f);
    valid = wena_wip_evaluate(&list->wip_limit, active_count, 0, 0, &status) &&
        wena_wip_evaluate(&list->wip_limit, active_count, 0, 1, &addition);
    /* Icons on the first row, where WeKan has them. */
    nk_layout_space_push(context, nk_rect(collapsed ? 10.0f : 21.0f, 25.0f, 22.0f, 22.0f));
    if (wena_wekan_icon_button(context, collapsed ? WENA_ICON_CARET_RIGHT : WENA_ICON_CARET_DOWN,
            wena_ui_text(collapsed ? WENA_UI_TEXT_UNCOLLAPSE : WENA_UI_TEXT_COLLAPSE), 14.0f, WENA_WEKAN_ICON)
        && collapse_clicked) *collapse_clicked = 1;
    if (!collapsed) {
        nk_layout_space_push(context, nk_rect(area.w - 104.0f, 25.0f, 22.0f, 22.0f));
        if (wena_wekan_icon_button(context, WENA_ICON_PLUS, wena_ui_text(WENA_UI_TEXT_ADD_CARD_TOP), 14.0f,
                valid && addition.allowed ? WENA_WEKAN_ICON : WENA_WEKAN_LIST_BORDER) && valid && addition.allowed)
            action |= WENA_LIST_HEADER_ADD_CARD;
        nk_layout_space_push(context, nk_rect(area.w - 76.0f, 25.0f, 22.0f, 22.0f));
        if (wena_wekan_icon_button(context, WENA_ICON_PLUS_SQUARE, wena_ui_text(WENA_UI_TEXT_ADD_LIST), 15.0f,
                WENA_WEKAN_ICON))
            action |= WENA_LIST_HEADER_ADD_LIST;
        nk_layout_space_push(context, nk_rect(area.w - 48.0f, 24.0f, 24.0f, 24.0f));
        if (wena_wekan_icon_button(context, WENA_ICON_BARS, wena_ui_text(WENA_UI_TEXT_LIST_ACTIONS), 18.0f,
                WENA_WEKAN_ICON))
            action |= WENA_LIST_HEADER_OPEN_MENU;
    }
    /* The title, bold, under the icons; the WIP count at its right. */
    title_width = area.w - 44.0f;
    if (valid && list->wip_limit.enabled) {
        sprintf(count, "%lu / %lu", (unsigned long)active_count, (unsigned long)list->wip_limit.value);
        nk_layout_space_push(context, nk_rect(area.w - 70.0f, 50.0f, 56.0f, 22.0f));
        wena_wekan_text(context, count, WENA_WEKAN_FONT_BOLD,
                        status.exceeded ? WENA_WEKAN_WIP_EXCEEDED : title_color, NK_TEXT_RIGHT);
        title_width -= 60.0f;
    }
    /* A collapsed list is a narrow strip with only its caret, as in WeKan. */
    if (!collapsed) {
        const struct nk_user_font *bold = wena_wekan_font(context, WENA_WEKAN_FONT_BOLD);
        /* A title wider than the list wraps, as WeKan's h2.list-header-name. */
        if (bold != NULL && bold->width(bold->userdata, bold->height, list->title,
                                        (int)strlen(list->title)) > title_width) {
            nk_layout_space_push(context, nk_rect(21.0f, 46.0f, title_width, 50.0f));
            wena_wekan_text_wrap(context, list->title, WENA_WEKAN_FONT_BOLD, title_color);
        } else {
            nk_layout_space_push(context, nk_rect(21.0f, 50.0f, title_width, 22.0f));
            wena_wekan_text(context, list->title, WENA_WEKAN_FONT_BOLD, title_color, NK_TEXT_LEFT);
        }
    }
    if (title_area) *title_area = nk_rect(area.x, area.y + 48.0f, area.w, area.h - 48.0f);
    nk_layout_space_end(context);
    return action;
}
