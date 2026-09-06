#include "../client/features/board.h"
#include "../client/platform/sdl_nuklear.h"

#include <assert.h>
#include <string.h>

static float test_text_width(nk_handle handle, float height,
                             const char *text, int length)
{
    (void)handle;
    (void)text;
    return height * (float)length * 0.5f;
}

int main(void)
{
    struct nk_context context;
    struct nk_user_font font;
    WenaBoard board;
    WenaBoardLayout layout;

    memset(&font, 0, sizeof(font));
    font.height = 13.0f;
    font.width = test_text_width;
    assert(nk_init_default(&context, &font));
    assert(wena_board_init(&board, "board", "Nuklear board", 0));
    memset(&layout, 0, sizeof(layout));
    layout.board = &board;
    assert(wena_board_feature_render(&context, &layout, 640.0f, 480.0f));
    assert(context.current == NULL);
    nk_free(&context);
    return 0;
}
