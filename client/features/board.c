#include "board.h"

#include <nuklear.h>

int wena_board_feature_render(struct nk_context *context,
                              const WenaBoardLayout *layout,
                              float width, float height)
{
    int rendered;

    if (context == NULL || layout == NULL || width <= 0.0f || height <= 0.0f) {
        return 0;
    }
    rendered = 0;
    if (nk_begin(context, "WeKan", nk_rect(0.0f, 0.0f, width, height),
                 NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)) {
        rendered = wena_board_layout_render(context, layout);
    }
    nk_end(context);
    return rendered;
}
