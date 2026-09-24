#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_IMPLEMENTATION
#include <nuklear.h>
#include "../client/features/board.h"
#include "../client/components/common/card_section.h"

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

static void wrapped_title(struct nk_context *context, const char *title)
{
    const struct nk_command *command;
    const struct nk_command_text *text;
    size_t consumed;
    int lines;
    short previous_y;

    consumed = 0;
    lines = 0;
    previous_y = -1;
    nk_foreach(command, context) {
        if (command->type != NK_COMMAND_TEXT) continue;
        text = (const struct nk_command_text *)command;
        if (text->length <= 0 || (size_t)text->length > strlen(title) - consumed ||
            memcmp(text->string, title + consumed, (size_t)text->length) != 0) continue;
        assert(text->y > previous_y);
        assert(text->x >= 0 && text->y > 0 && text->y + text->h <= 480);
        previous_y = text->y;
        consumed += (size_t)text->length;
        ++lines;
        if (consumed == strlen(title)) break;
    }
    assert(consumed == strlen(title) && lines >= 2);
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

static int fold_card(struct nk_context *context,void *opaque,const WenaCard *card)
{
    nk_layout_row_dynamic(context,24,1);
    return wena_card_section_toggle(context,(WenaCardSectionControl *)opaque,
        card->board_id,card->id,"minicard");
}
static unsigned int card_extra(struct nk_context *context,void *opaque,const WenaCard *card)
{
    (void)card;
    nk_layout_row_dynamic(context,24,1);
    nk_label(context,(const char *)opaque,NK_TEXT_LEFT);
    return 0;
}
static void card_folding(struct nk_context *context,WenaBoardLayout *layout)
{
    WenaCardSectionControl controls;
    WenaCardSectionsSnapshot preferences;
    WenaCardSectionPreference entry;
    memset(&controls,0,sizeof(controls));
    memset(&preferences,0,sizeof(preferences));
    memset(&entry,0,sizeof(entry));
    strcpy(preferences.board_id,layout->board->id);
    controls.snapshot=&preferences;
    layout->card_collapsed=fold_card;layout->card_collapsed_context=&controls;
    layout->card_badges=card_extra;layout->card_badges_context="Badge preview";
    layout->card_contents=card_extra;layout->card_contents_context="Expanded preview";
    render(context,layout,480);
    assert(visible_text(context,"Badge preview",640,480,NULL));
    assert(visible_text(context,"Expanded preview",640,480,NULL));
    click(context,layout,"Collapse");
    assert(controls.pending&&controls.collapsed&&!strcmp(controls.card_id,layout->cards[0].id));
    /* Only the first card is in the selected scope for this test. */
    assert(visible_text(context,"First card",640,480,NULL));
    assert(visible_text(context,"Open card",640,480,NULL));
    assert(visible_text(context,"Card menu",640,480,NULL));
    assert(!visible_text(context,"Badge preview",640,480,NULL));
    assert(!visible_text(context,"Expanded preview",640,480,NULL));
    strcpy(entry.card_id,controls.card_id);strcpy(entry.key,controls.key);
    entry.collapsed=1;entry.version=1;
    preferences.entries=&entry;preferences.count=1;preferences.capacity=1;
    controls.pending=0;
    render(context,layout,480);click(context,layout,"Uncollapse");
    assert(controls.pending&&!controls.collapsed&&controls.version==1);
    assert(visible_text(context,"Expanded preview",640,480,NULL));
    controls.pending=0;controls.readonly=1;
    render(context,layout,480);click(context,layout,"Uncollapse");assert(!controls.pending);
    assert(!visible_text(context,"Expanded preview",640,480,NULL));
    layout->card_collapsed=NULL;layout->card_collapsed_context=NULL;
    layout->card_badges=NULL;layout->card_contents=NULL;
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
    WenaBoardSidebar sidebar;
    WenaCard original_cards[3];
    WenaList original_lists[2];
    WenaSwimlane original_lanes[2];
    struct nk_window *menu;
    struct nk_vec2 first;
    struct nk_vec2 second;

    memset(&font, 0, sizeof(font));
    font.height = 14.0f;
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
    layout.card_count = 1;
    card_folding(&context, &layout);
    layout.card_count = 3;
    /* The original state-free caller also needs full-height list/card geometry. */
    render(&context, &layout, 480.0f);
    assert(visible_text(&context, "First card", 640.0f, 480.0f, &first));
    assert(visible_text(&context, "Local card", 640.0f, 480.0f, &second));
    assert(second.x > first.x + 200.0f);
    assert(visible_text(&context, "Add card", 640.0f, 480.0f, NULL));
    assert(visible_text(&context, "List menu", 640.0f, 480.0f, NULL));
    assert(visible_text(&context, "Open card", 640.0f, 480.0f, NULL));
    assert(visible_text(&context, "Card menu", 640.0f, 480.0f, NULL));
    strcpy(cards[0].title, "A long card title stays readable across several lines in its native column");
    render(&context, &layout, 480.0f);
    wrapped_title(&context, cards[0].title);
    assert(visible_text(&context, "Open card", 640.0f, 480.0f, NULL));
    strcpy(cards[0].title, "First card");
    strcpy(lists[0].title, "A long list title also has its own full width above the action buttons");
    render(&context, &layout, 480.0f);
    wrapped_title(&context, lists[0].title);
    assert(visible_text(&context, "Add card", 640.0f, 480.0f, NULL));
    strcpy(lists[0].title, "Same list");
    render(&context, &layout, 480.0f);
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
    /* The optional board menu must be in the viewport even below-fold lanes
       fill the scrolling board. Real mouse clicks traverse its section state. */
    memcpy(original_cards, cards, sizeof(cards));
    memcpy(original_lists, lists, sizeof(lists));
    memcpy(original_lanes, lanes, sizeof(lanes));
    wena_board_sidebar_init(&sidebar);
    layout.sidebar = &sidebar;
    layout.sidebar_as_window = 1;
    render(&context, &layout, 480.0f);
    click(&context, &layout, "Board menu");
    assert(sidebar.visible && sidebar.section == WENA_SIDEBAR_ACTIVITIES);
    assert(visible_text(&context, "Activities", 640.0f, 480.0f, NULL));
    assert(visible_text(&context, "Archives", 640.0f, 480.0f, NULL));
    menu = nk_window_find(&context, "Wena board menu");
    assert(menu != NULL && menu->bounds.x >= 0.0f && menu->bounds.y >= 0.0f);
    assert(menu->bounds.x + menu->bounds.w <= 640.0f);
    assert(menu->bounds.y + menu->bounds.h <= 480.0f);
    click(&context, &layout, "Archives");
    assert(sidebar.visible && sidebar.section == WENA_SIDEBAR_ARCHIVES);
    assert(visible_text(&context, "No archived items", 640.0f, 480.0f, NULL));
    click(&context, &layout, "Close");
    assert(!sidebar.visible);
    assert(!visible_text(&context, "Archives", 640.0f, 480.0f, NULL));
    click(&context, &layout, "Board menu");
    assert(sidebar.visible);
    /* Narrow windows clamp the panel to the viewport, with scrollable content. */
    nk_clear(&context); nk_input_begin(&context); nk_input_end(&context);
    assert(wena_board_feature_render(&context, &layout, 320.0f, 300.0f));
    menu = nk_window_find(&context, "Wena board menu");
    assert(menu != NULL && menu->bounds.x == 0.0f && menu->bounds.w == 320.0f);
    assert(menu->bounds.y == 44.0f && menu->bounds.h == 256.0f);
    assert(visible_text(&context, "Activities", 320.0f, 300.0f, NULL));
    assert(visible_text(&context, "Archives", 320.0f, 300.0f, NULL));
    assert(memcmp(original_cards, cards, sizeof(cards)) == 0);
    assert(memcmp(original_lists, lists, sizeof(lists)) == 0);
    assert(memcmp(original_lanes, lanes, sizeof(lanes)) == 0);
    nk_free(&context);
    puts("real Nuklear board layout tests passed");
    return 0;
}
