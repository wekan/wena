#include "card_destination.h"
#include "../../imports/ui/page_contract.h"
#include <nuklear.h>
#include <string.h>
int wena_card_destination_init(WenaCardDestination *state,size_t size,
    WenaDirectoryLoad load,void *context)
{
    if (!state) return 0;
    memset(state,0,sizeof(*state));
    return wena_directory_picker_init(&state->picker,size,load,context);
}
void wena_card_destination_close(WenaCardDestination *state)
{
    if (!state) return;
    wena_directory_picker_close(&state->picker);
    state->board_id[0]=0;state->selected=0;memset(&state->card,0,sizeof(state->card));
}
int wena_card_destination_open(WenaCardDestination *state,const char *board_id)
{
    WenaId scope;
    if (!state || !wena_model_identifier_valid(board_id)) return 0;
    strcpy(scope,board_id);
    wena_card_destination_close(state);
    if (!wena_directory_picker_open_scoped(&state->picker,WENA_DIRECTORY_CARDS,scope)) return 0;
    strcpy(state->board_id,scope);return 1;
}
int wena_card_destination_poll(WenaCardDestination *state)
{
    return state ? wena_directory_picker_poll(&state->picker) : 0;
}
int wena_card_destination_render(struct nk_context *context,WenaCardDestination *state)
{
    if (!context || !state || !state->picker.open) return 0;
    nk_layout_row_dynamic(context,28,2);
    if (nk_button_label(context,wena_ui_text(WENA_UI_TEXT_BOARDS))) {
        state->selected=0;state->board_id[0]=0;memset(&state->card,0,sizeof(state->card));
        (void)wena_directory_picker_open(&state->picker,WENA_DIRECTORY_BOARDS);
    }
    nk_label(context,state->board_id,NK_TEXT_LEFT);
    if (!wena_directory_picker_render(context,&state->picker)) return 0;
    if (state->picker.kind==WENA_DIRECTORY_BOARDS) {
        strcpy(state->board_id,state->picker.selected.id);
        state->selected=0;memset(&state->card,0,sizeof(state->card));
        (void)wena_directory_picker_open_scoped(&state->picker,WENA_DIRECTORY_CARDS,state->board_id);
        return 0;
    }
    state->card=state->picker.selected;state->selected=1;
    state->picker.selection_pending=0;
    return 1;
}
