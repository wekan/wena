#include "card_section.h"
#include "../../../imports/ui/page_contract.h"
#include <nuklear.h>
#include <string.h>
int wena_card_section_toggle(struct nk_context *context,WenaCardSectionControl *control,
    const char *board,const char *card,const char *key)
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
    if (valid && !control->readonly && !control->error) {
        if (nk_button_label(context,label) && !control->pending) {
            strcpy(control->card_id,card);strcpy(control->key,key);
            control->version=version;control->collapsed=!collapsed;control->pending=1;
            collapsed=control->collapsed;
        }
    } else nk_label(context,label,NK_TEXT_LEFT);
    return collapsed;
}
