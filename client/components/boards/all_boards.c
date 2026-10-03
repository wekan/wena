#include "all_boards.h"
#include "../common/wekan_look.h"
#include "../../../imports/ui/page_contract.h"

#include "../../platform/nuklear_options.h"
#include <nuklear.h>
#include <stdio.h>
#include <string.h>

/* WeKan's All Boards at 1024x720: an 85px header, the left menu 260px wide
 * with 251x46 rows 48px apart from y=121, the pane title at (283, 95), and
 * 234x126 tiles 8px apart from (284, 133). */
#define HEADER 85.0f
#define MENU_WIDTH 260.0f
#define ROW_TOP 121.0f
#define ROW_STEP 48.0f
#define TILE_LEFT 284.0f
#define TILE_TOP 133.0f
#define TILE_WIDTH 234.0f
#define TILE_HEIGHT 126.0f
#define TILE_GAP 8.0f
#define RAW(rgb) (0x1000000 | (rgb))

int wena_all_boards_in_section(const WenaWekanBoardTile *tile, WenaAllBoardsSection section)
{
    if (tile == NULL) return 0;
    switch (section) {
    case WENA_ALL_BOARDS_REMAINING: return !tile->archived && !tile->template_board;
    case WENA_ALL_BOARDS_STARRED: return !tile->archived && tile->starred;
    case WENA_ALL_BOARDS_TEMPLATES: return !tile->archived && tile->template_board;
    case WENA_ALL_BOARDS_ARCHIVE: return tile->archived;
    /* Home is a workspace of WeKan's; Wena keeps none yet. */
    default: return 0;
    }
}

int wena_all_boards_color(const char *name)
{
    /* client/components/boards/boardColors.css: --board-theme-accent, and
     * the background of the themes without one. */
    static const struct { const char *name; int rgb; } colors[] = {
        {"belize", 0x2980b9}, {"nephritis", 0x27ae60}, {"pomegranate", 0xc0392b}, {"pumpkin", 0xe67e22},
        {"wisteria", 0x8e44ad}, {"moderatepink", 0xcd5a91}, {"strongcyan", 0x00aecc}, {"limegreen", 0x4bbf6b},
        {"midnight", 0x2c3e50}, {"corteza", 0x568ba2}, {"clearblue", 0x499bea}, {"cleargreen", 0x8ad59f},
        {"clearorange", 0xefab6f}, {"clearpink", 0xdf94b8}, {"clearpurple", 0xb685ca}, {"clearred", 0xd67e75},
        {"moderndark", 0x454545}, {"exodark", 0x2b2b2b}, {"cleandark", 0x2e2e39}, {"cleanlight", 0xe0e0e0},
        {"dark", 0x2c3e50}};
    size_t index;
    if (name != NULL)
        for (index = 0; index < sizeof(colors) / sizeof(colors[0]); ++index)
            if (!strcmp(colors[index].name, name)) return colors[index].rgb;
    return 0x2980b9;
}

static const WenaUiTextId section_text[WENA_ALL_BOARDS_SECTION_COUNT] = {
    WENA_UI_TEXT_REMAINING, WENA_UI_TEXT_STARRED, WENA_UI_TEXT_HOME, WENA_UI_TEXT_TEMPLATES, WENA_UI_TEXT_ARCHIVES};
static const WenaIcon section_icon[WENA_ALL_BOARDS_SECTION_COUNT] = {
    WENA_ICON_FOLDER, WENA_ICON_STAR, WENA_ICON_HOME, WENA_ICON_CLIPBOARD, WENA_ICON_ARCHIVE};

/* Text in a color: a WenaWekanColor or RAW(rgb). */
static void label(struct nk_context *context, const char *text, WenaWekanFont font, int color,
                  float x, float y, float w, float h)
{
    const struct nk_user_font *face = wena_wekan_font(context, font);
    if (face == NULL || text == NULL) return;
    wena_wekan_draw_wrapped(context, text, font, color, x, y + (h - face->height) / 2.0f, w, face->height);
}

static float text_width(struct nk_context *context, WenaWekanFont font, const char *text)
{
    const struct nk_user_font *face = wena_wekan_font(context, font);
    return face != NULL && text != NULL ? face->width(face->userdata, face->height, text, (int)strlen(text)) : 0.0f;
}

unsigned int wena_all_boards_render(struct nk_context *context, WenaAllBoardsView *view,
                                    const WenaWekanBoardTile *tiles, size_t count, const char *user,
                                    float width, float height, char *board, size_t capacity)
{
    unsigned int action = WENA_ALL_BOARDS_NO_ACTION;
    struct nk_rect area;
    size_t counts[WENA_ALL_BOARDS_SECTION_COUNT], index, shown;
    int section, columns, column, row;
    float x, y, right, visible, content;
    char number[16];
    if (context == NULL || view == NULL || (count > 0 && tiles == NULL) || board == NULL || capacity == 0) return 0;
    board[0] = '\0';
    if ((int)view->section < 0 || view->section >= WENA_ALL_BOARDS_SECTION_COUNT) view->section = WENA_ALL_BOARDS_REMAINING;
    for (section = 0; section < WENA_ALL_BOARDS_SECTION_COUNT; ++section) {
        counts[section] = 0;
        for (index = 0; index < count; ++index)
            if (wena_all_boards_in_section(&tiles[index], (WenaAllBoardsSection)section)) ++counts[section];
    }
    nk_style_push_style_item(context, &context->style.window.fixed_background,
                             nk_style_item_color(nk_rgb((wena_wekan_rgb(WENA_WEKAN_BODY) >> 16) & 255,
                                                        (wena_wekan_rgb(WENA_WEKAN_BODY) >> 8) & 255,
                                                        wena_wekan_rgb(WENA_WEKAN_BODY) & 255)));
    nk_style_push_vec2(context, &context->style.window.padding, nk_vec2(0.0f, 0.0f));
    nk_style_push_vec2(context, &context->style.window.spacing, nk_vec2(0.0f, 0.0f));
    if (nk_begin(context, "All Boards", nk_rect(0.0f, 0.0f, width, height), NK_WINDOW_NO_SCROLLBAR)) {
        wena_ui_region("all-boards");
        nk_layout_space_begin(context, NK_STATIC, height, 1024);
        wena_wekan_space_area(context, height, &area.x, &area.y, &area.w);
        area.h = height;

        /* The header: the house that is All Boards, its name, the user. */
        wena_ui_region("header");
        wena_wekan_fill(context, area.x, area.y, area.w, HEADER, WENA_WEKAN_HEADER, 0.0f);
        nk_layout_space_push(context, nk_rect(16.0f, 16.0f, 32.0f, 24.0f));
        (void)wena_wekan_icon_button(context, WENA_ICON_HOME, wena_ui_text(WENA_UI_TEXT_ALL_BOARDS), 18.0f,
                                     WENA_WEKAN_HEADER_TEXT);
        label(context, wena_ui_text(WENA_UI_TEXT_ALL_BOARDS), WENA_WEKAN_FONT_BODY, WENA_WEKAN_HEADER_TEXT,
              area.x + 60.0f, area.y + 14.0f, 160.0f, 28.0f);
        if (user != NULL && user[0] != '\0') {
            /* WeKan's avatar - initials in a gray circle - and the name. */
            float user_width = text_width(context, WENA_WEKAN_FONT_SMALL, user) + 4.0f;
            if (user_width > 200.0f) user_width = 200.0f;
            wena_wekan_fill(context, area.x + area.w - user_width - 61.0f, area.y + 54.0f, 1.0f, 20.0f,
                            WENA_WEKAN_HEADER_LINK, 0.0f);
            nk_layout_space_push(context, nk_rect(area.w - user_width - 51.0f, 51.0f, 24.0f, 24.0f));
            wena_wekan_avatar(context, user);
            nk_layout_space_push(context, nk_rect(area.w - user_width - 22.0f, 50.0f, user_width, 25.0f));
            (void)wena_wekan_link(context, WENA_ICON_NONE, user, WENA_WEKAN_FONT_SMALL, WENA_WEKAN_HEADER_LINK);
        }

        /* The left menu: the sections and how many boards each has. */
        wena_ui_region("all-boards-menu");
        wena_wekan_fill(context, area.x, area.y + HEADER, MENU_WIDTH, area.h - HEADER, WENA_WEKAN_PANEL, 0.0f);
        wena_wekan_icon_draw(context, WENA_ICON_CARET_DOWN, area.x + 16.0f, area.y + HEADER + 13.0f, 12.0f,
                             WENA_WEKAN_ICON);
        for (section = 0; section < WENA_ALL_BOARDS_SECTION_COUNT; ++section) {
            int active = (int)view->section == section;
            float row_y = ROW_TOP + ROW_STEP * (float)section;
            const char *name = wena_ui_text(section_text[section]);
            nk_layout_space_push(context, nk_rect(4.0f, row_y, 251.0f, 46.0f));
            if (nk_input_is_mouse_hovering_rect(&context->input, nk_rect(area.x + 4.0f, area.y + row_y, 251.0f, 46.0f))
                && !active)
                wena_wekan_fill(context, area.x + 4.0f, area.y + row_y, 251.0f, 46.0f, RAW(0xffffff), 0.0f);
            if (active) wena_wekan_fill(context, area.x + 4.0f, area.y + row_y, 251.0f, 46.0f, WENA_WEKAN_BUTTON, 0.0f);
            if (wena_wekan_area(context, name)) {
                view->section = (WenaAllBoardsSection)section;
                view->scroll = 0.0f;
            }
            wena_wekan_icon_draw(context, section_icon[section], area.x + 18.0f, area.y + row_y + 16.0f, 14.0f,
                                 active ? WENA_WEKAN_BUTTON_TEXT : WENA_WEKAN_ICON_ACTIVE);
            label(context, name, WENA_WEKAN_FONT_BODY, active ? WENA_WEKAN_BUTTON_TEXT : WENA_WEKAN_TEXT,
                  area.x + 40.0f, area.y + row_y, 170.0f, 46.0f);
            /* The count, in its pill: white over blue when active, #ddd else. */
            wena_wekan_fill(context, area.x + 222.0f, area.y + row_y + 12.0f, 23.0f, 22.0f,
                            active ? RAW(0x69a6ce) : RAW(0xdddddd), 11.0f);
            sprintf(number, "%lu", (unsigned long)counts[section]);
            label(context, number, WENA_WEKAN_FONT_BOLD, active ? WENA_WEKAN_BUTTON_TEXT : RAW(0x333333),
                  area.x + 222.0f + (23.0f - text_width(context, WENA_WEKAN_FONT_BOLD, number)) / 2.0f,
                  area.y + row_y + 12.0f, 23.0f, 22.0f);
        }

        /* The section's boards. */
        wena_ui_region("all-boards");
        label(context, wena_ui_text(section_text[view->section]), WENA_WEKAN_FONT_TITLE, WENA_WEKAN_TEXT,
              area.x + 283.0f, area.y + 95.0f, area.w - 300.0f, 26.0f);
        right = area.w - 14.0f;
        columns = (int)((right - TILE_LEFT + TILE_GAP) / (TILE_WIDTH + TILE_GAP));
        if (columns < 1) columns = 1;
        shown = counts[view->section] +
                (view->section == WENA_ALL_BOARDS_REMAINING || view->section == WENA_ALL_BOARDS_TEMPLATES ? 1 : 0);
        content = (float)((shown + (size_t)columns - 1) / (size_t)columns) * (TILE_HEIGHT + TILE_GAP);
        visible = area.h - TILE_TOP - 8.0f;
        if (nk_input_is_mouse_hovering_rect(&context->input, nk_rect(area.x + MENU_WIDTH, area.y + HEADER,
                                                                    area.w - MENU_WIDTH, area.h - HEADER)))
            view->scroll -= context->input.mouse.scroll_delta.y * 40.0f;
        if (view->scroll > content - visible) view->scroll = content - visible;
        if (view->scroll < 0.0f) view->scroll = 0.0f;
        column = 0; row = 0;
        if (view->section == WENA_ALL_BOARDS_REMAINING || view->section == WENA_ALL_BOARDS_TEMPLATES) {
            float add_width = text_width(context, WENA_WEKAN_FONT_SECTION, wena_ui_text(WENA_UI_TEXT_ADD_BOARD)) + 22.0f;
            y = TILE_TOP - view->scroll;
            if (y + TILE_HEIGHT > HEADER) {
                nk_layout_space_push(context, nk_rect(TILE_LEFT, y, TILE_WIDTH, TILE_HEIGHT));
                wena_wekan_fill(context, area.x + TILE_LEFT, area.y + y, TILE_WIDTH, TILE_HEIGHT, RAW(0x999999), 3.0f);
                if (wena_wekan_area(context, wena_ui_text(WENA_UI_TEXT_ADD_BOARD))) action |= WENA_ALL_BOARDS_ADD;
                wena_wekan_icon_draw(context, WENA_ICON_PLUS, area.x + TILE_LEFT + (TILE_WIDTH - add_width) / 2.0f,
                                     area.y + y + TILE_HEIGHT / 2.0f - 8.0f, 16.0f, WENA_WEKAN_BUTTON_TEXT);
                label(context, wena_ui_text(WENA_UI_TEXT_ADD_BOARD), WENA_WEKAN_FONT_SECTION, WENA_WEKAN_BUTTON_TEXT,
                      area.x + TILE_LEFT + (TILE_WIDTH - add_width) / 2.0f + 22.0f, area.y + y, add_width, TILE_HEIGHT);
            }
            column = 1;
        }
        for (index = 0; index < count; ++index) {
            const WenaWekanBoardTile *tile = &tiles[index];
            int star_clicked;
            if (!wena_all_boards_in_section(tile, view->section)) continue;
            if (column == columns) { column = 0; ++row; }
            x = TILE_LEFT + (float)column * (TILE_WIDTH + TILE_GAP);
            y = TILE_TOP + (float)row * (TILE_HEIGHT + TILE_GAP) - view->scroll;
            ++column;
            if (y + TILE_HEIGHT <= HEADER || y >= area.h) continue;
            wena_ui_region("board-tile");
            wena_wekan_fill(context, area.x + x, area.y + y, TILE_WIDTH, TILE_HEIGHT,
                            RAW(wena_all_boards_color(tile->color)), 0.0f);
            /* The star first: a click on it is not a click on the board. */
            nk_layout_space_push(context, nk_rect(x + TILE_WIDTH - 34.0f, y, 34.0f, 36.0f));
            star_clicked = wena_wekan_icon_button(context, tile->starred ? WENA_ICON_STAR : WENA_ICON_STAR_O,
                                                  wena_ui_text(WENA_UI_TEXT_STAR_BOARD_TITLE), 16.0f, RAW(0xf6f6f6));
            if (star_clicked && strlen(tile->id) < capacity) {
                strcpy(board, tile->id);
                action |= WENA_ALL_BOARDS_STAR;
            }
            nk_layout_space_push(context, nk_rect(x, y, TILE_WIDTH - 34.0f, TILE_HEIGHT));
            if (wena_wekan_area(context, tile->title) && !star_clicked && tile->openable && strlen(tile->id) < capacity) {
                strcpy(board, tile->id);
                action |= WENA_ALL_BOARDS_OPEN;
            }
            wena_wekan_draw_wrapped(context, tile->title, WENA_WEKAN_FONT_SECTION, RAW(0xf6f6f6),
                                    area.x + x + 8.0f, area.y + y + 24.0f, TILE_WIDTH - 16.0f - 26.0f, 75.0f);
        }
        nk_layout_space_end(context);
        wena_wekan_tooltip_flush(context);
    }
    nk_end(context);
    nk_style_pop_vec2(context);
    nk_style_pop_vec2(context);
    nk_style_pop_style_item(context);
    return action;
}
