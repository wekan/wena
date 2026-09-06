#include "../client/features/board.h"
#include "../client/components/boards/board_header.h"

#include <assert.h>
#include <nuklear.h>
#include <string.h>

struct nk_rect nk_rect(float x, float y, float w, float h)
{
    struct nk_rect rectangle;

    rectangle.x = x;
    rectangle.y = y;
    rectangle.w = w;
    rectangle.h = h;
    return rectangle;
}

int nk_begin(struct nk_context *context, const char *title,
             struct nk_rect bounds, unsigned int flags)
{
    (void)title;
    (void)bounds;
    (void)flags;
    ++context->begin_count;
    return 1;
}

void nk_end(struct nk_context *context)
{
    ++context->end_count;
}

void nk_layout_row_dynamic(struct nk_context *context, float height, int columns)
{
    (void)context;
    (void)height;
    (void)columns;
}

void nk_layout_row_begin(struct nk_context *context, int format,
                         float row_height, int columns)
{
    (void)context;
    (void)format;
    (void)row_height;
    (void)columns;
}

void nk_layout_row_push(struct nk_context *context, float value)
{
    (void)context;
    (void)value;
}

void nk_layout_row_end(struct nk_context *context)
{
    (void)context;
}

void nk_label(struct nk_context *context, const char *text, int alignment)
{
    (void)alignment;
    context->labels[context->label_count++] = text;
}

int nk_button_label(struct nk_context *context, const char *title)
{
    int result;

    (void)title;
    ++context->button_count;
    result = context->next_button_result;
    context->next_button_result = 0;
    return result;
}

int nk_group_begin(struct nk_context *context, const char *title,
                   unsigned int flags)
{
    (void)title;
    (void)flags;
    ++context->group_depth;
    return 1;
}

void nk_group_end(struct nk_context *context)
{
    --context->group_depth;
}

int main(void)
{
    WenaBoard board;
    WenaSwimlane swimlanes[2];
    WenaList lists[2];
    WenaCard cards[3];
    WenaBoardLayout layout;
    struct nk_context context;

    memset(&context, 0, sizeof(context));
    assert(wena_board_init(&board, "board", "Project", 0));
    assert(wena_swimlane_init(&swimlanes[0], "lane", "board", "Current", 1.0, 0));
    assert(wena_swimlane_init(&swimlanes[1], "hidden", "board", "Hidden", 2.0, 1));
    assert(wena_list_init(&lists[0], "doing", "board", "lane", "Doing", 1.0, 0));
    assert(wena_list_init(&lists[1], "old", "board", "lane", "Old", 2.0, 1));
    assert(wena_card_init(&cards[0], "one", "board", "lane", "doing", "One", 1.0, 0));
    assert(wena_card_init(&cards[1], "two", "board", "lane", "doing", "Two", 2.0, 0));
    assert(wena_card_init(&cards[2], "old", "board", "lane", "doing", "Old", 3.0, 1));

    layout.board = &board;
    layout.swimlanes = swimlanes;
    layout.swimlane_count = 2;
    layout.lists = lists;
    layout.list_count = 2;
    layout.cards = cards;
    layout.card_count = 3;

    assert(wena_board_feature_render(&context, &layout, 800.0f, 600.0f));
    assert(context.begin_count == 1);
    assert(context.end_count == 1);
    assert(context.group_depth == 0);
    assert(context.button_count == 1);
    assert(context.label_count == 5);
    assert(strcmp(context.labels[0], "Project") == 0);
    assert(strcmp(context.labels[1], "Current") == 0);
    assert(strcmp(context.labels[2], "Doing") == 0);
    assert(strcmp(context.labels[3], "One") == 0);
    assert(strcmp(context.labels[4], "Two") == 0);
    assert(!wena_board_feature_render(&context, &layout, 0.0f, 600.0f));
    board.archived = 1;
    assert(!wena_board_layout_render(&context, &layout));
    board.archived = 0;
    layout.cards = NULL;
    assert(!wena_board_layout_render(&context, &layout));
    layout.cards = cards;
    context.next_button_result = 1;
    assert(wena_board_header_render(&context, &board) ==
           WENA_BOARD_HEADER_OPEN_MENU);
    assert(wena_board_header_render(NULL, &board) ==
           WENA_BOARD_HEADER_NO_ACTION);
    return 0;
}
