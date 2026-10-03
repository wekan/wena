/* Dragging the bar below a swimlane changes the lane's height, in the real
 * Nuklear: press, drag, release stores it in the collapse state; Escape
 * cancels; it clamps; dragging back to the default removes the entry; a
 * collapsed lane and a layout without the bar have no bar. */
#include "../client/platform/nuklear_options.h"
#define NK_IMPLEMENTATION
#include <nuklear.h>
#include "../client/components/boards/swimlane_resize.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct Fixture {
    struct nk_context ctx;
    struct nk_user_font font;
    WenaBoard board;
    WenaSwimlane lanes[2];
    WenaList list;
    WenaBoardCollapseState collapse;
    WenaSwimlaneResize resize;
    WenaBoardLayout layout;
    float bars[4];
    size_t bar_count;
} Fixture;

static float text_width(nk_handle handle, float height, const char *text, int length)
{
    (void)handle; (void)text;
    return height * (float)length * 0.5f;
}

/* One frame at `y` with the button `down`; records each bar's centre. */
static void frame(Fixture *f, float y, int down, int escape)
{
    const struct nk_command *command;
    nk_clear(&f->ctx);
    nk_input_begin(&f->ctx);
    nk_input_motion(&f->ctx, 200, (int)y);
    nk_input_button(&f->ctx, NK_BUTTON_LEFT, 200, (int)y, down);
    nk_input_key(&f->ctx, NK_KEY_TEXT_RESET_MODE, escape);
    nk_input_end(&f->ctx);
    if (nk_begin(&f->ctx, "Board", nk_rect(0, 0, 900, 4000), NK_WINDOW_BORDER))
        assert(wena_board_layout_render(&f->ctx, &f->layout));
    nk_end(&f->ctx);
    f->bar_count = 0;
    nk_foreach(command, &f->ctx) {
        if (command->type == NK_COMMAND_RECT_FILLED) {
            const struct nk_command_rect_filled *rect = (const struct nk_command_rect_filled *)command;
            if (rect->rounding == 2 && (rect->h == 2 || rect->h == 4) && rect->w > 100) {
                assert(f->bar_count < 4);
                f->bars[f->bar_count++] = (float)rect->y + (float)rect->h / 2.0f;
            }
        }
    }
}

static void drag(Fixture *f, size_t bar, float by, int escape)
{
    float y = f->bars[bar];
    frame(f, y, 0, 0);
    assert(f->resize.hovered);
    frame(f, y, 1, 0);
    assert(f->resize.active && !strcmp(f->resize.swimlane_id, f->lanes[bar].id));
    frame(f, y + by, 1, 0);
    if (escape) { frame(f, y + by, 1, 1); assert(!f->resize.active); frame(f, y + by, 0, 0); return; }
    frame(f, y + by, 0, 0);
    assert(!f->resize.active);
    frame(f, 3900, 0, 0);
}

int main(void)
{
    static Fixture f;
    unsigned int before;
    float first_gap, first_bar;

    memset(&f, 0, sizeof(f));
    f.font.height = 14; f.font.width = text_width;
    assert(nk_init_default(&f.ctx, &f.font));
    assert(wena_board_init(&f.board, "board", "Board", 0));
    assert(wena_swimlane_init(&f.lanes[0], "lane-a", "board", "A", 0, 0));
    assert(wena_swimlane_init(&f.lanes[1], "lane-b", "board", "B", 1, 0));
    assert(wena_list_init(&f.list, "list", "board", "", "List", 0, 1));
    wena_board_collapse_init(&f.collapse);
    f.layout.board = &f.board;
    f.layout.swimlanes = f.lanes; f.layout.swimlane_count = 2;
    f.layout.lists = &f.list; f.layout.list_count = 1;
    f.layout.collapse = &f.collapse;
    f.layout.swimlane_resize = &f.resize;
    f.layout.swimlane_resize_bar = wena_swimlane_resize_bar;

    frame(&f, 3900, 0, 0);
    assert(f.bar_count == 2);
    assert(!f.resize.hovered);
    first_gap = f.bars[1] - f.bars[0];
    first_bar = f.bars[0];

    /* Drag the first bar down 120 pixels: lane A is 480, lane B unchanged. */
    drag(&f, 0, 120.0f, 0);
    assert(wena_board_swimlane_height(&f.collapse, "board", "lane-a") == WENA_SWIMLANE_HEIGHT_DEFAULT + 120);
    assert(wena_board_swimlane_height(&f.collapse, "board", "lane-b") == WENA_SWIMLANE_HEIGHT_DEFAULT);
    assert(f.collapse.height_count == 1);
    /* Lane A really is drawn taller: its bar sits 120 pixels lower, and lane B
     * below it keeps its height, so the gap to the next bar is unchanged. */
    assert(f.bars[0] == first_bar + 120.0f);
    assert(f.bars[1] - f.bars[0] == first_gap);

    /* Escape cancels: the height stays as it was. */
    before = wena_board_swimlane_height(&f.collapse, "board", "lane-a");
    drag(&f, 0, -200.0f, 1);
    assert(wena_board_swimlane_height(&f.collapse, "board", "lane-a") == before);

    /* Clamped to the minimum and maximum. */
    drag(&f, 1, -2000.0f, 0);
    assert(wena_board_swimlane_height(&f.collapse, "board", "lane-b") == WENA_SWIMLANE_HEIGHT_MIN);
    drag(&f, 1, 3000.0f, 0);
    assert(wena_board_swimlane_height(&f.collapse, "board", "lane-b") == WENA_SWIMLANE_HEIGHT_MAX);

    /* Dragging back to the default removes the entry. */
    drag(&f, 0, -120.0f, 0);
    assert(wena_board_swimlane_height(&f.collapse, "board", "lane-a") == WENA_SWIMLANE_HEIGHT_DEFAULT);
    assert(f.collapse.height_count == 1 && !strcmp(f.collapse.height_ids[0], "lane-b"));

    /* Negative: a collapsed lane has no bar, a press beside a bar starts
     * nothing, and a layout without the bar draws none. */
    assert(wena_board_collapse_set(&f.collapse, &f.layout, WENA_COLLAPSE_SWIMLANE, "lane-a", 1));
    frame(&f, 3900, 0, 0);
    assert(f.bar_count == 1);
    frame(&f, f.bars[0] + 30.0f, 0, 0); frame(&f, f.bars[0] + 30.0f, 1, 0);
    assert(!f.resize.active);
    frame(&f, 3900, 0, 0);
    f.layout.swimlane_resize_bar = NULL;
    frame(&f, 3900, 0, 0);
    assert(f.bar_count == 0);

    nk_free(&f.ctx);
    puts("swimlane resize: drag, release, escape, clamps, default and negatives passed");
    return 0;
}
