#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_IMPLEMENTATION
#include <nuklear.h>
#include "../client/features/board.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static float text_width(nk_handle handle, float height,
                        const char *text, int length)
{
    (void)handle;
    (void)text;
    return height * (float)length * 0.5f;
}

/* Inspect actual renderer commands and the scissor effective at each text. */
static int visible_text(struct nk_context *context, const char *label,
                         float width, float height, struct nk_vec2 *point)
{
    const struct nk_command *command;
    struct nk_rect clip;
    const struct nk_command_text *text;
    const struct nk_command_scissor *scissor;

    clip = nk_rect(0.0f, 0.0f, width, height);
    nk_foreach(command, context) {
        if (command->type == NK_COMMAND_SCISSOR) {
            scissor = (const struct nk_command_scissor *)command;
            clip = nk_rect((float)scissor->x, (float)scissor->y,
                            (float)scissor->w, (float)scissor->h);
        } else if (command->type == NK_COMMAND_TEXT) {
            text = (const struct nk_command_text *)command;
            if ((size_t)text->length == strlen(label) &&
                memcmp(text->string, label, (size_t)text->length) == 0 &&
                text->x >= 0 && text->y > 0 && text->w > 0 && text->h > 0 &&
                (float)text->x < width && (float)text->y < height &&
                (float)text->x < clip.x + clip.w &&
                (float)text->y < clip.y + clip.h &&
                (float)text->x + (float)text->w > clip.x &&
                (float)text->y + (float)text->h > clip.y) {
                if (point != NULL) {
                    *point = nk_vec2((float)text->x + (float)text->w * 0.5f,
                                      (float)text->y + (float)text->h * 0.5f);
                }
                return 1;
            }
        }
    }
    return 0;
}

static void render(struct nk_context *context, WenaBoardLayout *layout,
                    float height)
{
    nk_clear(context);
    nk_input_begin(context);
    nk_input_end(context);
    assert(wena_board_feature_render(context, layout, 640.0f, height));
    assert(context->current == NULL);
}

static void click(struct nk_context *context, WenaBoardLayout *layout,
                   const char *label)
{
    struct nk_vec2 point;
    int down;

    assert(visible_text(context, label, 640.0f, 480.0f, &point));
    for (down = 1; down >= 0; --down) {
        nk_clear(context);
        nk_input_begin(context);
        nk_input_motion(context, (int)point.x, (int)point.y);
        nk_input_button(context, NK_BUTTON_LEFT, (int)point.x, (int)point.y, down);
        nk_input_end(context);
        assert(wena_board_feature_render(context, layout, 640.0f, 480.0f));
    }
    render(context, layout, 480.0f);
}

int main(void)
{
    struct nk_context context;
    struct nk_user_font font;
    WenaBoard board;
    WenaSwimlane lanes[2];
    WenaList lists[2];
    WenaCard cards[3];
    WenaBoardLayout layout;
    WenaBoardCollapseState collapse;
    struct nk_vec2 first;
    struct nk_vec2 second;

    memset(&font, 0, sizeof(font));
    font.height = 13.0f;
    font.width = text_width;
    assert(nk_init_default(&context, &font));
    assert(wena_board_init(&board, "board", "Board", 0));
    assert(wena_swimlane_init(&lanes[0], "first", "board", "Same lane", 1.0, 0));
    assert(wena_swimlane_init(&lanes[1], "second", "board", "Same lane", 2.0, 0));
    assert(wena_list_init(&lists[0], "shared", "board", "", "Same list", 1.0, 0));
    assert(wena_list_init(&lists[1], "local", "board", "first", "Same list", 2.0, 0));
    assert(wena_card_init(&cards[0], "one", "board", "first", "shared", "First card", 1.0, 0));
    assert(wena_card_init(&cards[1], "two", "board", "second", "shared", "Second card", 1.0, 0));
    assert(wena_card_init(&cards[2], "three", "board", "first", "local", "Local card", 1.0, 0));
    memset(&layout, 0, sizeof(layout));
    layout.board = &board;
    layout.swimlanes = lanes;
    layout.swimlane_count = 2;
    layout.lists = lists;
    layout.list_count = 2;
    layout.cards = cards;
    layout.card_count = 3;
    /* The original state-free caller also needs full-height list/card geometry. */
    render(&context, &layout, 480.0f);
    assert(visible_text(&context, "First card", 640.0f, 480.0f, &first));
    assert(visible_text(&context, "Local card", 640.0f, 480.0f, &second));
    assert(second.x > first.x + 200.0f);
    assert((nk_window_find(&context, "WeKan")->flags & NK_WINDOW_NO_SCROLLBAR) == 0);

    wena_board_collapse_init(&collapse);
    layout.collapse = &collapse;
    render(&context, &layout, 480.0f);
    assert(visible_text(&context, "First card", 640.0f, 480.0f, NULL));
    click(&context, &layout, "Collapse");
    assert(wena_board_is_collapsed(&collapse, "board", WENA_COLLAPSE_SWIMLANE, "first"));
    assert(!visible_text(&context, "First card", 640.0f, 480.0f, NULL));
    assert(!visible_text(&context, "Local card", 640.0f, 480.0f, NULL));
    assert(visible_text(&context, "Second card", 640.0f, 480.0f, NULL));
    click(&context, &layout, "Uncollapse");
    assert(!wena_board_is_collapsed(&collapse, "board", WENA_COLLAPSE_SWIMLANE, "first"));
    assert(visible_text(&context, "First card", 640.0f, 480.0f, NULL));
    assert(wena_board_collapse_set(&collapse, &layout, WENA_COLLAPSE_LIST, "shared", 1));
    render(&context, &layout, 900.0f);
    assert(!visible_text(&context, "First card", 640.0f, 900.0f, NULL));
    assert(!visible_text(&context, "Second card", 640.0f, 900.0f, NULL));
    assert(visible_text(&context, "Local card", 640.0f, 900.0f, NULL));
    assert(wena_board_collapse_set(&collapse, &layout, WENA_COLLAPSE_LIST, "shared", 0));
    render(&context, &layout, 900.0f);
    assert(visible_text(&context, "First card", 640.0f, 900.0f, &first));
    assert(visible_text(&context, "Second card", 640.0f, 900.0f, &second));
    assert(second.y > first.y + 300.0f);
    nk_free(&context);
    puts("real Nuklear board layout tests passed");
    return 0;
}
