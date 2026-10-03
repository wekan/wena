#include "card_section.h"
#include "wekan_look.h"
#include "../../../imports/ui/page_contract.h"
#include "../../platform/nuklear_options.h"
#include <nuklear.h>
#include <string.h>
static int toggle(struct nk_context *context,WenaCardSectionControl *control,
    const char *board,const char *card,const char *key,int caret)
{
    const WenaCardSectionPreference *preference;
    const char *label;
    int collapsed,valid;
    unsigned long version;
    if (!context || !control || !wena_model_identifier_valid(board) ||
        !wena_model_identifier_valid(card) || !wena_card_section_key_valid(key)) return 0;
    valid=control->snapshot && !strcmp(control->snapshot->board_id,board);
    preference=valid?wena_card_sections_find(control->snapshot,card,key):NULL;
    collapsed=preference?preference->collapsed:0;version=preference?preference->version:0;
    if (control->pending && !strcmp(control->card_id,card) && !strcmp(control->key,key))
        collapsed=control->collapsed;
    label=wena_ui_control_text(collapsed?WENA_UI_EXPAND_LIST:WENA_UI_COLLAPSE_LIST);
    if (caret) {
        /* WeKan's minicard caret: down while open, right while collapsed. */
        label=wena_ui_text(collapsed?WENA_UI_TEXT_UNCOLLAPSE:WENA_UI_TEXT_COLLAPSE);
        if (wena_wekan_icon_button(context,collapsed?WENA_ICON_CARET_RIGHT:WENA_ICON_CARET_DOWN,label,12.0f,
                WENA_WEKAN_ICON) && valid && !control->readonly && !control->error && !control->pending) {
            strcpy(control->card_id,card);strcpy(control->key,key);
            control->version=version;control->collapsed=!collapsed;control->pending=1;
            collapsed=control->collapsed;
        }
        return collapsed;
    }
    if (valid && !control->readonly && !control->error) {
        if (nk_button_label(context,label) && !control->pending) {
            strcpy(control->card_id,card);strcpy(control->key,key);
            control->version=version;control->collapsed=!collapsed;control->pending=1;
            collapsed=control->collapsed;
        }
    } else nk_label(context,label,NK_TEXT_LEFT);
    return collapsed;
}
int wena_card_section_toggle(struct nk_context *context,WenaCardSectionControl *control,
    const char *board,const char *card,const char *key)
{
    return toggle(context,control,board,card,key,0);
}
int wena_card_section_toggle_caret(struct nk_context *context,WenaCardSectionControl *control,
    const char *board,const char *card,const char *key)
{
    return toggle(context,control,board,card,key,1);
}
