#include "entry_form.h"
#include "../../components/forms/text_form.h"
#include "../../components/forms/input_limits.h"
#include "../../../imports/ui/page_contract.h"
#include "../../../models/checklist_item_titles.h"
#include <nuklear.h>
#include <stdio.h>
#include <string.h>

unsigned int wena_checklist_entry_form(struct nk_context *context,
    WenaChecklistAction *action,char *input,int *length,int capacity,int *error)
{
    int split,limit;
    size_t count;
    char progress[64];
    char titles[WENA_CHECKLIST_BATCH_MAX_ITEMS][WENA_CHECKLIST_TITLE_CAPACITY];
    if (!context || !action || !input || !length || !error ||
        *length<0 || *length>=capacity) return 0;
    if (*action==WENA_CHECKLIST_ADD_ITEM || *action==WENA_CHECKLIST_ADD_ITEMS) {
        nk_layout_row_dynamic(context,28,1);
        split=*action==WENA_CHECKLIST_ADD_ITEMS;
        if (nk_checkbox_label(context,wena_ui_text(WENA_UI_TEXT_CHECKLIST_SPLIT_LINES),&split)) {
            /* Never truncate or drop multiline text when changing modes. */
            if (!split && (*length>=WENA_CHECKLIST_TITLE_CAPACITY ||
                memchr(input,'\n',(size_t)*length) || memchr(input,'\r',(size_t)*length)))
                *error=1;
            else *action=split ? WENA_CHECKLIST_ADD_ITEMS : WENA_CHECKLIST_ADD_ITEM;
        }
    }
    split=*action==WENA_CHECKLIST_ADD_ITEMS;
    if (split) {
        count=0;
        (void)wena_checklist_item_batch_parse(input,(size_t)*length,titles,&count);
        sprintf(progress,"%lu / %lu",(unsigned long)count,
            (unsigned long)WENA_CHECKLIST_BATCH_MAX_ITEMS);
        nk_label(context,wena_ui_text(WENA_UI_TEXT_CHECKLIST_WITH_ITEMS),NK_TEXT_LEFT);
        nk_label(context,progress,NK_TEXT_LEFT);
    }
    limit=split ? capacity : WENA_NATIVE_EDIT_CAPACITY(WENA_CHECKLIST_TITLE_CAPACITY);
    if (limit>capacity) limit=capacity;
    return wena_text_form_render_mode(context,input,length,limit,0,split);
}
