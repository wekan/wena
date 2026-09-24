#include "card_selection_panel.h"
#include "../components/cards/card_body.h"
#include "../components/forms/text_form.h"
#include "../../imports/ui/page_contract.h"
#include <nuklear.h>
#include <stdio.h>
#include <stdlib.h>
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
    if(rows->panel->captured){
        if(row>=rows->panel->captured_count)return NULL;
        for(i=0;i<rows->count;++i)if(!strcmp(rows->cards[i].id,rows->panel->captured[row].id)&&
            !strcmp(rows->cards[i].board_id,rows->panel->selection->board_id))return &rows->cards[i];
        return NULL;
    }
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
    if(rows->panel->captured){nk_label(context,label,NK_TEXT_LEFT);return 0;}
    return nk_checkbox_label(context,label,&checked)?1u:0u;
}
void wena_card_selection_panel_init(WenaCardSelectionPanel *panel,WenaCardSelection *selection)
{
    if(!panel)return;
    memset(panel,0,sizeof(*panel));panel->selection=selection;(void)wena_table_init(&panel->table,4);
}
static void cancel_capture(WenaCardSelectionPanel *panel)
{free(panel->captured);panel->captured=NULL;panel->captured_count=0;panel->archive_error=0;panel->table.page=0;}
void wena_card_selection_panel_hide(WenaCardSelectionPanel *panel)
{if(panel){cancel_capture(panel);panel->visible=0;}}
void wena_card_selection_panel_set_archive(WenaCardSelectionPanel *panel,
    WenaSelectionCapture capture,WenaSelectionArchive archive,void *context)
{
    if(!panel)return;
    wena_card_selection_panel_hide(panel);panel->capture=capture;panel->archive=archive;panel->archive_context=context;
}
static int capture_matches(const WenaCardSelectionPanel *panel)
{
    size_t i;
    if(!panel->captured||!panel->captured_count||panel->captured_count!=panel->selection->count||
        panel->captured_count>WENA_CARD_SELECTION_CAPACITY)return 0;
    for(i=0;i<panel->captured_count;++i)if(!wena_model_identifier_valid(panel->captured[i].id)||
        strcmp(panel->captured[i].id,panel->selection->ids[i])||!panel->captured[i].version||
        panel->captured[i].version>WENA_VERSION_MUTATE_MAX)return 0;
    return 1;
}
void wena_card_selection_panel_close(WenaCardSelectionPanel *panel)
{
    if(!panel)return;
    cancel_capture(panel);wena_card_selection_clear(panel->selection);
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
    cancel_capture(panel);
    strcpy(panel->list_id,list_id);strcpy(panel->lane_id,lane_id);panel->visible=1;panel->error=0;panel->table.page=0;return 1;
}
int wena_card_selection_panel_render(struct nk_context *context,WenaCardSelectionPanel *panel,
    const WenaCard *cards,size_t count,const char *board,float width,float height)
{
    SelectionRows rows;WenaTableView view;WenaTableResult result;size_t i,total;
    const WenaCard *card;char selected[32];int clear,all,close,archive,confirm,cancel;
    if(!panel||!panel->visible||!panel->selection)return 0;
    if(!wena_model_identifier_valid(board)||strcmp(panel->selection->board_id,board)){
        wena_card_selection_panel_close(panel);return 0;
    }
    if(!context||width<=0||height<=0)return 0;
    panel->error=!titles_valid(cards,count)||(panel->captured?!capture_matches(panel):!wena_card_selection_sync(panel->selection,cards,count));
    rows.panel=panel;rows.cards=cards;rows.count=count;clear=all=close=archive=confirm=cancel=0;total=0;
    if(panel->captured)total=panel->captured_count;
    else if(!panel->error)for(i=0;i<count;++i)if(in_scope(&rows,i))++total;
    if(nk_begin_titled(context,"Card selection",wena_ui_text(WENA_UI_TEXT_MULTI_SELECTION),
        nk_rect(width*0.2f,0,width*0.8f,height),NK_WINDOW_BORDER)){
        if(wena_text_form_keys(context,0u)&WENA_TEXT_FORM_CANCEL){
            nk_end(context);if(panel->captured)cancel_capture(panel);else wena_card_selection_panel_close(panel);return 1;
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
        if(!panel->error&&!panel->captured){
            nk_layout_row_dynamic(context,28,2);
            all=nk_button_label(context,wena_ui_text(WENA_UI_TEXT_SELECT_ALL));
            clear=nk_button_label(context,wena_ui_text(WENA_UI_TEXT_SELECT_NONE));
        }
        if(panel->captured){
            nk_layout_row_dynamic(context,28,2);
            if(!panel->error)confirm=nk_button_label(context,wena_ui_text(WENA_UI_TEXT_MOVE_TO_ARCHIVE));
            else nk_label(context,wena_ui_text(WENA_UI_TEXT_MOVE_TO_ARCHIVE),NK_TEXT_LEFT);
            cancel=nk_button_label(context,wena_ui_control_text(WENA_UI_CANCEL));
        }else if(!panel->error&&panel->selection->count&&panel->capture&&panel->archive){
            nk_layout_row_dynamic(context,28,1);archive=nk_button_label(context,wena_ui_text(WENA_UI_TEXT_ARCHIVE_SELECTION));
        }
        if(panel->archive_error){nk_layout_row_dynamic(context,48,1);nk_label_wrap(context,wena_ui_text(WENA_UI_TEXT_OPERATION_FAILED));}
        nk_layout_row_dynamic(context,28,1);
        close=nk_button_label(context,wena_ui_text(WENA_UI_TEXT_MULTI_SELECTION_OFF));
    }
    nk_end(context);
    if(close)wena_card_selection_panel_close(panel);
    else if(cancel)cancel_capture(panel);
    else if(confirm){
        if(capture_matches(panel)&&panel->archive&&panel->archive(panel->archive_context,board,panel->captured,panel->captured_count))
            wena_card_selection_panel_close(panel);
        else panel->archive_error=1;
    }else if(archive){
        if(panel->capture(panel->archive_context,board,(const WenaId*)panel->selection->ids,panel->selection->count,&panel->captured)){
            panel->captured_count=panel->selection->count;panel->table.page=0;panel->archive_error=0;
            if(!capture_matches(panel)){cancel_capture(panel);panel->archive_error=1;}
        }else panel->archive_error=1;
    }
    else if(clear){wena_card_selection_clear(panel->selection);panel->archive_error=0;}
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


void wena_card_selection_traversal_begin(WenaCardSelectionTraversal *state,WenaCardSelection *selection)
{
    if(!state)return;
    state->selection=selection;state->count=0;state->error=0;
    if(!selection||!wena_model_identifier_valid(selection->board_id)){state->error=1;return;}
    if(strcmp(state->board_id,selection->board_id)||
        !wena_card_selection_contains(selection,state->anchor))state->anchor[0]='\0';
    strcpy(state->board_id,selection->board_id);
}
unsigned int wena_card_selection_traversal_control(struct nk_context *context,void *data,const WenaCard *card)
{
    WenaCardSelectionTraversal *state;size_t i;unsigned int action;
    state=(WenaCardSelectionTraversal*)data;
    if(!context||!state||state->error||!card)return 0;
    if(!wena_model_identifier_valid(card->id)||card->archived||
        strcmp(card->board_id,state->board_id)||state->count>=WENA_CARD_SELECTION_CAPACITY){state->error=1;return 0;}
    for(i=0;i<state->count;++i)if(!strcmp(state->ids[i],card->id)){state->error=1;return 0;}
    strcpy(state->ids[state->count++],card->id);
    action=wena_card_selection_control(context,state->selection,card);
    if(action&&nk_input_is_key_down(&context->input,NK_KEY_SHIFT))return WENA_CARD_BODY_RANGE_SELECTION;
    return action;
}
int wena_card_selection_traversal_apply(WenaCardSelectionTraversal *state,const WenaCard *cards,
    size_t count,const char *target,unsigned int action)
{
    size_t i;int found,anchored;
    if(!state||state->error||!state->selection||!wena_model_identifier_valid(target)||
        strcmp(state->board_id,state->selection->board_id)||
        (action!=WENA_CARD_BODY_TOGGLE_SELECTION&&action!=WENA_CARD_BODY_RANGE_SELECTION))return 0;
    found=0;anchored=0;
    for(i=0;i<state->count;++i){
        if(!strcmp(state->ids[i],target))found=1;
        if(!strcmp(state->ids[i],state->anchor))anchored=1;
    }
    if(!found)return 0;
    if(action==WENA_CARD_BODY_RANGE_SELECTION&&anchored&&
        wena_card_selection_contains(state->selection,state->anchor))
        return wena_card_selection_range(state->selection,cards,count,(const WenaId*)state->ids,
            state->count,state->anchor,target);
    if(!wena_card_selection_toggle(state->selection,cards,count,target))return 0;
    if(wena_card_selection_contains(state->selection,target))strcpy(state->anchor,target);
    else state->anchor[0]='\0';
    return 1;
}

int wena_card_selection_selected(void *data,const WenaCard *card)
{
    WenaCardSelection *selection;selection=(WenaCardSelection*)data;
    return selection&&card&&!card->archived&&!strcmp(selection->board_id,card->board_id)&&
        wena_card_selection_contains(selection,card->id);
}
