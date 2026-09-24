#include "card_archives.h"
#include "../../imports/ui/page_contract.h"
#include <nuklear.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

typedef struct ArchiveOptions {
    const WenaBoardLayout *layout;
    WenaArchiveKind kind;
    char label[WENA_TITLE_CAPACITY + WENA_ID_CAPACITY + 8];
} ArchiveOptions;

static int layout_valid(const WenaBoardLayout *layout)
{
    return layout != NULL && layout->board != NULL && !layout->board->archived &&
        (layout->card_count == 0 || layout->cards != NULL) &&
        layout->card_count <= (size_t)(INT_MAX / 64) &&
        (layout->list_count == 0 || layout->lists != NULL) &&
        layout->list_count <= (size_t)(INT_MAX / 64) &&
        (layout->swimlane_count == 0 || layout->swimlanes != NULL) &&
        layout->swimlane_count <= (size_t)(INT_MAX / 64);
}

typedef struct ArchiveItem { const char *id;const char *title; } ArchiveItem;

static int item_at(const WenaBoardLayout *layout,WenaArchiveKind kind,size_t i,ArchiveItem *item)
{
    const char *board;int archived;
    if(kind==WENA_ARCHIVE_SWIMLANES){item->id=layout->swimlanes[i].id;item->title=layout->swimlanes[i].title;
        board=layout->swimlanes[i].board_id;archived=layout->swimlanes[i].archived;}
    else if(kind==WENA_ARCHIVE_LISTS){item->id=layout->lists[i].id;item->title=layout->lists[i].title;
        board=layout->lists[i].board_id;archived=layout->lists[i].archived;}
    else{item->id=layout->cards[i].id;item->title=layout->cards[i].title;
        board=layout->cards[i].board_id;archived=layout->cards[i].archived;}
    return archived==1&&!strcmp(board,layout->board->id);
}
static size_t item_count(const WenaBoardLayout *layout,WenaArchiveKind kind)
{return kind==WENA_ARCHIVE_SWIMLANES?layout->swimlane_count:kind==WENA_ARCHIVE_LISTS?layout->list_count:layout->card_count;}
static const WenaUiTextId category_text[WENA_ARCHIVE_KIND_COUNT]={
    WENA_UI_TEXT_CARDS,WENA_UI_TEXT_LISTS,WENA_UI_TEXT_SWIMLANES};
static const WenaUiTextId empty_text[WENA_ARCHIVE_KIND_COUNT]={
    WENA_UI_TEXT_NO_ARCHIVED_CARDS,WENA_UI_TEXT_NO_ARCHIVED_LISTS,WENA_UI_TEXT_NO_ARCHIVED_SWIMLANES};
static int option_item(const WenaBoardLayout *layout,WenaArchiveKind kind,int selected,ArchiveItem *item)
{
    size_t i,size;int index;size=item_count(layout,kind);index=0;
    for(i=0;i<size;++i)if(item_at(layout,kind,i,item)&&index++==selected)return 1;
    return 0;
}

static unsigned int archive_row(struct nk_context *context,void *data,size_t index)
{
    ArchiveOptions *options;
    ArchiveItem item;
    options=(ArchiveOptions*)data;
    if (!option_item(options->layout,options->kind,(int)index,&item)) { nk_label(context,wena_ui_text(WENA_UI_TEXT_UNKNOWN),NK_TEXT_LEFT);return 0; }
    sprintf(options->label,"%s [%s]",item.title,item.id);
    return nk_button_label(context,options->label) ? 1u : 0u;
}

static void select_item(WenaCardArchivesState *state,const WenaBoardLayout *layout,int selected)
{
    char title[WENA_TITLE_CAPACITY];ArchiveItem item;int loaded;
    state->version = 0;
    state->error = 0;
    state->card_id[0] = '\0';
    if (!option_item(layout,state->kind,selected,&item)) return;
    loaded=wena_model_set_required(state->card_id,sizeof(state->card_id),item.id);
    if(loaded)loaded=state->kind ?
        (state->providers[state->kind].load&&state->providers[state->kind].load(state->providers[state->kind].context,state->board_id,state->card_id,&state->version)) :
        (state->load&&state->load(state->context,state->board_id,state->card_id,title,sizeof(title),&state->version));
    if (!loaded || state->version == 0) {
        state->version = 0;
        state->error = 1;
    }
}

void wena_card_archives_init(WenaCardArchivesState *state,
    WenaCardDetailsLoadTitle load, WenaCardArchivesRestore restore, void *context)
{
    if (state == NULL) return;
    memset(state, 0, sizeof(*state));
    (void)wena_table_init(&state->table,4);
    state->load = load; state->restore = restore; state->context = context;
}

void wena_card_archives_close(WenaCardArchivesState *state)
{
    if (state == NULL) return;
    state->kind = 0;
    state->table.page = 0;
    state->visible = 0; state->error = 0; state->version = 0;
    state->card_id[0] = '\0'; state->board_id[0] = '\0';
}

void wena_card_archives_set_lists(WenaCardArchivesState *state,
    WenaArchivesLoadVersion load,WenaCardArchivesRestore restore,void *context)
{wena_card_archives_set_provider(state,WENA_ARCHIVE_LISTS,load,restore,context);}
void wena_card_archives_set_provider(WenaCardArchivesState *state,WenaArchiveKind kind,
    WenaArchivesLoadVersion load,WenaCardArchivesRestore restore,void *context)
{
    if(!state||kind<=WENA_ARCHIVE_CARDS||kind>=WENA_ARCHIVE_KIND_COUNT)return;
    wena_card_archives_close(state);
    state->providers[kind].load=load;state->providers[kind].restore=restore;state->providers[kind].context=context;
}

int wena_card_archives_open(WenaCardArchivesState *state,
    const WenaBoardLayout *layout)
{
    if (state == NULL) return 0;
    wena_card_archives_close(state);
    if (!layout_valid(layout) || !wena_model_set_required(state->board_id,
        sizeof(state->board_id), layout->board->id)) return 0;
    state->visible = 1;
    select_item(state,layout,0);
    return 1;
}

int wena_card_archives_render(struct nk_context *context,
    WenaCardArchivesState *state, const WenaBoardLayout *layout,
    float width, float height)
{
    ArchiveOptions options;
    ArchiveItem item;
    WenaTableView view;
    WenaTableResult result;
    size_t i;
    int count, selected, close_requested;
    if (state == NULL || !state->visible) return 0;
    if (state->kind<WENA_ARCHIVE_CARDS || state->kind>=WENA_ARCHIVE_KIND_COUNT ||
        !layout_valid(layout) || strcmp(state->board_id, layout->board->id)) {
        wena_card_archives_close(state); return 0;
    }
    if (context == NULL || width <= 0 || height <= 0) return 0;
    close_requested = 0;
    if (nk_begin_titled(context, "Archives", wena_ui_text(WENA_UI_TEXT_ARCHIVES), nk_rect(width * 0.5f, 0,
        width * 0.5f, height), NK_WINDOW_BORDER)) {
        /* Escape cancels the entire focused panel, including an open selector.
         * Check before widgets so it cannot share a frame with a mutation. */
        if ((wena_title_input_keys(context, 0u) & WENA_TITLE_INPUT_CANCEL) != 0u) {
            nk_end(context);
            wena_card_archives_close(state);
            return 1;
        }
        nk_layout_row_dynamic(context, 28, 1);
        nk_label(context, wena_ui_text(WENA_UI_TEXT_ARCHIVES), NK_TEXT_LEFT);
        {
            int categories,k;WenaArchiveKind choice;categories=1;choice=state->kind;
            for(k=1;k<WENA_ARCHIVE_KIND_COUNT;++k)
                if(state->providers[k].load&&state->providers[k].restore)++categories;
            if(categories>1){
                nk_layout_row_dynamic(context,28,categories);
                for(k=0;k<WENA_ARCHIVE_KIND_COUNT;++k)
                    if(k==0||(state->providers[k].load&&state->providers[k].restore))
                        if(nk_button_label(context,wena_ui_text(category_text[k])))choice=(WenaArchiveKind)k;
                if(choice!=state->kind){state->kind=choice;state->table.page=0;select_item(state,layout,0);}
                nk_layout_row_dynamic(context,24,1);
                nk_label(context,wena_ui_text(category_text[state->kind]),NK_TEXT_LEFT);
            }
        }
        count=0;selected=-1;
        for(i=0;i<item_count(layout,state->kind);++i){
            if(!item_at(layout,state->kind,i,&item))continue;
            if(!strcmp(item.id,state->card_id))selected=count;
            ++count;
        }
        if(selected<0){select_item(state,layout,0);selected=count?0:-1;}
        options.layout=layout;options.kind=state->kind;
        memset(&view,0,sizeof(view));
        view.row_count=(size_t)count;view.column_count=1;view.row_height=28;
        view.render_row=archive_row;view.context=&options;
        view.empty_text=wena_ui_text(empty_text[state->kind]);
        result=wena_table_render(context,&state->table,&view);
        if (result.action && (int)result.row!=selected) {
            selected=(int)result.row;
            select_item(state,layout,selected);
        }
        if (count != 0) {
            /* Keep the restore target visible even after navigating away from
             * its page. Rows return an intent; restoration remains explicit. */
            nk_layout_row_dynamic(context,28,1);
            if (option_item(layout,state->kind,selected,&item)) {
                sprintf(options.label,"%s [%s]",item.title,item.id);
                nk_label(context,options.label,NK_TEXT_LEFT);
            } else nk_label(context,"",NK_TEXT_LEFT);
            nk_layout_row_dynamic(context, 28, 1);
            if (nk_button_label(context, wena_ui_control_text(WENA_UI_RESTORE_CARD))) {
                if (state->version != 0 && (state->kind ?
                    (state->providers[state->kind].restore && state->providers[state->kind].restore(state->providers[state->kind].context,state->board_id,state->card_id,state->version)) :
                    (state->restore && state->restore(state->context,state->board_id,state->card_id,state->version)))) {
                    select_item(state,layout,0);
                } else state->error = 1;
            }
        }
        nk_layout_row_dynamic(context, 28, 2);
        if (nk_button_label(context, wena_ui_control_text(WENA_UI_CANCEL))) close_requested = 1;
        if (nk_button_label(context, wena_ui_control_text(WENA_UI_CLOSE))) close_requested = 1;
        if (state->error) {
                nk_layout_row_dynamic(context, 48.0f, 1);
                nk_label_wrap(context, wena_ui_text(WENA_UI_TEXT_OPERATION_FAILED));
            }
    }
    nk_end(context);
    if (close_requested) wena_card_archives_close(state);
    return 1;
}
