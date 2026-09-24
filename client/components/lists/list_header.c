#include "../common/color_heading.h"
#include "list_header.h"
#include "../../../imports/ui/page_contract.h"

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
