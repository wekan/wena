#include "board_header.h"

#include <nuklear.h>

unsigned int wena_board_header_render(struct nk_context *context,
                                      const WenaBoard *board)
{
    unsigned int action;

    if (context == NULL || board == NULL || board->archived) {
        return WENA_BOARD_HEADER_NO_ACTION;
    }
    action = WENA_BOARD_HEADER_NO_ACTION;
    nk_layout_row_begin(context, NK_DYNAMIC, 34.0f, 2);
    nk_layout_row_push(context, 0.78f);
    nk_label(context, board->title, NK_TEXT_LEFT);
    nk_layout_row_push(context, 0.22f);
    if (nk_button_label(context, "Board menu")) {
        action |= WENA_BOARD_HEADER_OPEN_MENU;
    }
    nk_layout_row_end(context);
    return action;
}
