#include "card_selection_panel.h"
#include "../components/cards/card_body.h"
#include "../components/forms/text_form.h"
#include "../../imports/ui/page_contract.h"
#include <nuklear.h>
#include <stdio.h>
#include <string.h>
typedef struct SelectionRows {
    WenaCardSelectionPanel *panel;
    const WenaCard *cards;
    size_t count;
} SelectionRows;
static int titles_valid(const WenaCard *cards,size_t count)
{
    size_t i;
    if(count>WENA_CARD_SELECTION_CAPACITY||(!cards&&count))return 0;
    for(i=0;i<count;++i)if(!wena_model_title_string_valid(cards[i].title,sizeof(cards[i].title)))return 0;
    return 1;
}
static int in_scope(const SelectionRows *rows,size_t i)
{
    const WenaCard *card;card=&rows->cards[i];
    return !card->archived&&!strcmp(card->board_id,rows->panel->selection->board_id)&&
        !strcmp(card->list_id,rows->panel->list_id)&&
        (!rows->panel->lane_id[0]||!strcmp(card->swimlane_id,rows->panel->lane_id));
}
static const WenaCard *row_card(const SelectionRows *rows,size_t row)
{
    size_t i,found;found=0;
    for(i=0;i<rows->count;++i)if(in_scope(rows,i)&&found++==row)return &rows->cards[i];
    return NULL;
}
static unsigned int render_row(struct nk_context *context,void *data,size_t row)
{
    SelectionRows *rows;const WenaCard *card;int checked;char label[WENA_TITLE_CAPACITY+WENA_ID_CAPACITY+8];
    rows=(SelectionRows*)data;card=row_card(rows,row);
    if(!card){nk_label(context,wena_ui_text(WENA_UI_TEXT_UNKNOWN),NK_TEXT_LEFT);return 0;}
    checked=wena_card_selection_contains(rows->panel->selection,card->id);
    sprintf(label,"%s [%s]",card->title,card->id);
    return nk_checkbox_label(context,label,&checked)?1u:0u;
}
void wena_card_selection_panel_init(WenaCardSelectionPanel *panel,WenaCardSelection *selection)
{
    if(!panel)return;
    memset(panel,0,sizeof(*panel));panel->selection=selection;(void)wena_table_init(&panel->table,4);
}
void wena_card_selection_panel_close(WenaCardSelectionPanel *panel)
{
    if(!panel)return;
    wena_card_selection_clear(panel->selection);
    panel->visible=panel->error=0;panel->table.page=0;panel->list_id[0]=panel->lane_id[0]=0;
}
int wena_card_selection_panel_open(WenaCardSelectionPanel *panel,const WenaCard *cards,
    size_t count,const char *board,const char *list,const char *lane)
{
    WenaId list_id,lane_id;
    if(!panel||!panel->selection||!titles_valid(cards,count)||!wena_model_identifier_valid(board)||
        strcmp(panel->selection->board_id,board)||!wena_model_identifier_valid(list)||
        (lane&&lane[0]&&!wena_model_identifier_valid(lane)))return 0;
    strcpy(list_id,list);lane_id[0]=0;if(lane)strcpy(lane_id,lane);
    if(!wena_card_selection_add(panel->selection,cards,count,list_id,lane_id))return 0;
    strcpy(panel->list_id,list_id);strcpy(panel->lane_id,lane_id);panel->visible=1;panel->error=0;panel->table.page=0;return 1;
}
int wena_card_selection_panel_render(struct nk_context *context,WenaCardSelectionPanel *panel,
    const WenaCard *cards,size_t count,const char *board,float width,float height)
{
    SelectionRows rows;WenaTableView view;WenaTableResult result;size_t i,total;
    const WenaCard *card;char selected[32];int clear,all,close;
    if(!panel||!panel->visible||!panel->selection)return 0;
    if(!wena_model_identifier_valid(board)||strcmp(panel->selection->board_id,board)){
        wena_card_selection_panel_close(panel);return 0;
    }
    if(!context||width<=0||height<=0)return 0;
    panel->error=!titles_valid(cards,count)||!wena_card_selection_sync(panel->selection,cards,count);
    rows.panel=panel;rows.cards=cards;rows.count=count;clear=all=close=0;total=0;
    if(!panel->error)for(i=0;i<count;++i)if(in_scope(&rows,i))++total;
    if(nk_begin_titled(context,"Card selection",wena_ui_text(WENA_UI_TEXT_MULTI_SELECTION),
        nk_rect(width*0.2f,0,width*0.8f,height),NK_WINDOW_BORDER)){
        if(wena_text_form_keys(context,0u)&WENA_TEXT_FORM_CANCEL){
            nk_end(context);wena_card_selection_panel_close(panel);return 1;
        }
        nk_layout_row_dynamic(context,28,2);
        nk_label(context,wena_ui_text(WENA_UI_TEXT_CARDS),NK_TEXT_LEFT);
        sprintf(selected,"%lu",(unsigned long)panel->selection->count);
        nk_label(context,selected,NK_TEXT_LEFT);
        memset(&view,0,sizeof(view));view.row_count=total;view.column_count=1;view.row_height=28;
        view.empty_text=wena_ui_text(WENA_UI_TEXT_NO_ITEMS);
        view.error_text=panel->error?wena_ui_text(WENA_UI_TEXT_OPERATION_FAILED):NULL;
        view.render_row=render_row;view.context=&rows;result=wena_table_render(context,&panel->table,&view);
        if(result.action&&!panel->error){
            card=row_card(&rows,result.row);
            if(!card||!wena_card_selection_toggle(panel->selection,cards,count,card->id))panel->error=1;
        }
        if(!panel->error){
            nk_layout_row_dynamic(context,28,2);
            all=nk_button_label(context,wena_ui_text(WENA_UI_TEXT_SELECT_ALL));
            clear=nk_button_label(context,wena_ui_text(WENA_UI_TEXT_SELECT_NONE));
        }
        nk_layout_row_dynamic(context,28,1);
        close=nk_button_label(context,wena_ui_text(WENA_UI_TEXT_MULTI_SELECTION_OFF));
    }
    nk_end(context);
    if(close)wena_card_selection_panel_close(panel);
    else if(clear)wena_card_selection_clear(panel->selection);
    else if(all&&!wena_card_selection_add(panel->selection,cards,count,panel->list_id,panel->lane_id))panel->error=1;
    return 1;
}


unsigned int wena_card_selection_control(struct nk_context *context,void *data,const WenaCard *card)
{
    WenaCardSelection *selection;int checked;selection=(WenaCardSelection*)data;
    if(!context||!selection||!card||card->archived||
        strcmp(selection->board_id,card->board_id))return WENA_CARD_BODY_NO_ACTION;
    checked=wena_card_selection_contains(selection,card->id);
    nk_layout_row_dynamic(context,28,1);
    return nk_checkbox_label(context,wena_ui_text(WENA_UI_TEXT_SELECTED),&checked)?
        WENA_CARD_BODY_TOGGLE_SELECTION:WENA_CARD_BODY_NO_ACTION;
}
