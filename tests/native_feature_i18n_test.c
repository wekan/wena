#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_IMPLEMENTATION
#include <nuklear.h>
#include "../client/features/card_details.h"
#include "../client/features/card_create.h"
#include "../client/features/card_move.h"
#include "../client/features/card_archives.h"
#include "../client/features/hierarchy_title.h"
#include "../imports/i18n/ui_catalog.h"
#include "../imports/ui/page_contract.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static float width(nk_handle handle, float height, const char *text, int length)
{
    (void)handle; (void)text;
    return height * (float)length * 0.45f;
}
static int has_text(struct nk_context *ctx, const char *expected)
{
    const struct nk_command *command;
    nk_foreach(command, ctx) {
        const struct nk_command_text *text;
        if (command->type != NK_COMMAND_TEXT) continue;
        text = (const struct nk_command_text *)command;
        if ((size_t)text->length == strlen(expected) &&
            memcmp(text->string, expected, (size_t)text->length) == 0) return 1;
    }
    return 0;
}
static void failure(struct nk_context *ctx)
{
    assert(has_text(ctx, wena_ui_text(WENA_UI_TEXT_OPERATION_FAILED)));
    assert(!has_text(ctx, "[!]"));
}
static void initialize(struct nk_context *ctx, struct nk_user_font *font)
{
    assert(nk_init_default(ctx, font));
}
static int load(void *context, const char *board, const char *card,
    char *title, size_t capacity, unsigned long *version)
{
    (void)context; (void)board; (void)card; (void)title; (void)capacity;
    *version = 0;
    return 0;
}
int main(void)
{
    struct nk_context ctx;
    struct nk_user_font font;
    struct nk_window *window;
    WenaLanguageState language;
    WenaBoard board;
    WenaList list;
    WenaSwimlane lane;
    WenaCard cards[2];
    WenaBoardLayout layout;
    WenaCardDetailsState details;
    WenaCardCreateState create;
    WenaCardMoveState move;
    WenaCardArchivesState archives;
    WenaHierarchyTitleState hierarchy;
    memset(&font, 0, sizeof(font)); font.height = 12; font.width = width;
    assert(wena_board_init(&board, "b", "Board", 0));
    assert(wena_swimlane_init(&lane, "s", "b", "Lane", 0, 0));
    assert(wena_list_init(&list, "l", "b", "s", "List", 0, 0));
    assert(wena_card_init(&cards[0], "c", "b", "s", "l", "Card", 0, 0));
    assert(wena_card_init(&cards[1], "arch", "b", "s", "l", "Archived", 1, 1));
    memset(&layout, 0, sizeof(layout)); layout.board = &board;
    layout.lists = &list; layout.list_count = 1; layout.swimlanes = &lane;
    layout.swimlane_count = 1; layout.cards = cards; layout.card_count = 2;
    memset(&language, 0, sizeof(language)); strcpy(language.current, "fi");
    wena_ui_set_translator(wena_ui_catalog_translate, &language);
    assert(strcmp(wena_ui_text(WENA_UI_TEXT_OPERATION_FAILED), "Something went wrong") != 0);
    wena_card_details_init(&details);
    assert(wena_card_details_open(&details, &cards[0]));
    details.editing_title = 1; details.title_error = 1; details.title_version = 7;
    strcpy(details.title_input, "Draft"); details.title_length = 5;
    initialize(&ctx, &font);
    assert(wena_card_details_render(&ctx, &details, cards, 2, 1000, 700));
    failure(&ctx); assert(has_text(&ctx, "Tallenna"));
    window = nk_window_find(&ctx, "Card details"); assert(window != NULL);
    strcpy(language.current, "en"); nk_clear(&ctx);
    assert(wena_card_details_render(&ctx, &details, cards, 2, 1000, 700));
    failure(&ctx); assert(has_text(&ctx, "Save"));
    assert(nk_window_find(&ctx, "Card details") == window);
    assert(details.editing_title && details.title_version == 7);
    assert(strcmp(details.title_input, "Draft") == 0);
    nk_free(&ctx); strcpy(language.current, "fi");
    memset(&create, 0, sizeof(create)); create.visible = create.error = 1;
    strcpy(create.board_id, "b"); strcpy(create.list_id, "l"); strcpy(create.swimlane_id, "s");
    initialize(&ctx, &font);
    assert(wena_card_create_render(&ctx, &create, &layout, 1000, 700));
    failure(&ctx); assert(has_text(&ctx, wena_ui_control_text(WENA_UI_ADD_CARD))); nk_free(&ctx);
    memset(&move, 0, sizeof(move)); move.visible = move.error = 1;
    strcpy(move.board_id, "b"); strcpy(move.card_id, "c");
    strcpy(move.source_list_id, "l"); strcpy(move.source_swimlane_id, "s");
    /* Valid source, invalid destination lane: list selector has no options. */
    strcpy(move.target_swimlane_id, "absent");
    initialize(&ctx, &font);
    assert(wena_card_move_render(&ctx, &move, &layout, 1000, 700));
    failure(&ctx); assert(has_text(&ctx, wena_ui_text(WENA_UI_TEXT_NO_ITEMS))); nk_free(&ctx);
    wena_card_archives_init(&archives, load, NULL, NULL);
    assert(wena_card_archives_open(&archives, &layout));
    initialize(&ctx, &font);
    assert(wena_card_archives_render(&ctx, &archives, &layout, 1000, 700));
    failure(&ctx); assert(has_text(&ctx, wena_ui_text(WENA_UI_TEXT_ARCHIVES))); nk_free(&ctx);
    wena_hierarchy_title_init(&hierarchy); hierarchy.visible = hierarchy.error = 1;
    hierarchy.kind = WENA_HIERARCHY_BOARD; strcpy(hierarchy.board_id, "b");
    strcpy(hierarchy.target_id, "b");
    initialize(&ctx, &font);
    assert(wena_hierarchy_title_render(&ctx, &hierarchy, &layout, 1000, 700));
    failure(&ctx); nk_free(&ctx);
    wena_ui_set_translator(NULL, NULL);
    puts("native feature canonical messages and stable locale-switch state passed");
    return 0;
}
