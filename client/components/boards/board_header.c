#include "board_header.h"
#include "../common/wekan_look.h"
#include "../../../imports/ui/page_contract.h"

#include "../../platform/nuklear_options.h"
#include <nuklear.h>
#include <stdio.h>
#include <string.h>

static WenaBoardTitleRenderer title_renderer;

void wena_board_header_set_title_renderer(WenaBoardTitleRenderer renderer)
{
    title_renderer = renderer;
}

unsigned int wena_board_header_render(struct nk_context *context,
                                      const WenaBoard *board)
{
    return wena_board_header_render_info(context, board, NULL);
}

static float text_width(struct nk_context *context, WenaWekanFont font, const char *text)
{
    const struct nk_user_font *face = wena_wekan_font(context, font);
    return face != NULL && text != NULL ?
        face->width(face->userdata, face->height, text, (int)strlen(text)) : 0.0f;
}

unsigned int wena_board_header_render_info(struct nk_context *context,
                                           const WenaBoard *board,
                                           const WenaBoardHeaderInfo *info)
{
    unsigned int action;
    struct nk_rect area;
    float width, title_width, filter_width, filter_x, user_width, buttons_x, second_x = 16.0f;
    char tooltip[256];
    const char *filter;

    if (context == NULL || board == NULL || board->archived) {
        return WENA_BOARD_HEADER_NO_ACTION;
    }
    action = WENA_BOARD_HEADER_NO_ACTION;
    wena_ui_region("header");
    nk_layout_space_begin(context, NK_STATIC, WENA_BOARD_HEADER_HEIGHT, 25);
    wena_wekan_space_area(context, WENA_BOARD_HEADER_HEIGHT, &area.x, &area.y, &area.w);
    area.h = WENA_BOARD_HEADER_HEIGHT;
    width = area.w;
    wena_wekan_fill(context, area.x, area.y, area.w, area.h, WENA_WEKAN_HEADER, 0.0f);

    /* First row: All Boards' house when there is All Boards, then the board
     * title, as WeKan's header (the title at x=81 after the house). */
    if (info != NULL && info->all_boards) {
        nk_layout_space_push(context, nk_rect(16.0f, 16.0f, 32.0f, 24.0f));
        if (wena_wekan_icon_button(context, WENA_ICON_HOME, wena_ui_text(WENA_UI_TEXT_ALL_BOARDS), 18.0f,
                                   WENA_WEKAN_HEADER_TEXT))
            action |= WENA_BOARD_HEADER_ALL_BOARDS;
    }
    if (info != NULL && info->collapse) {
        nk_layout_space_push(context, nk_rect(52.0f, 14.0f, 25.0f, 28.0f));
        if (wena_wekan_icon_button(context, info->icons_collapsed ? WENA_ICON_ANGLES_RIGHT : WENA_ICON_ANGLES_LEFT,
                                   wena_ui_key_text("toggle-header-icons-collapsed", NULL), 13.0f, WENA_WEKAN_HEADER_LINK))
            action |= WENA_BOARD_HEADER_COLLAPSE_ICONS;
    }
    title_width = text_width(context, WENA_WEKAN_FONT_BODY, board->title) + 4.0f;
    /* A vector decorator draws its icon before the title: room for both. */
    if (title_renderer && context->style.font != NULL)
        title_width += context->style.font->height * 1.5f + 6.0f;
    if (title_width > width * 0.5f) title_width = width * 0.5f;
    nk_layout_space_push(context, nk_rect(info != NULL && info->all_boards ? 81.0f : 16.0f, 14.0f, title_width, 28.0f));
    buttons_x = (info != NULL && info->all_boards ? 81.0f : 16.0f) + title_width + 12.0f;
    if (title_renderer) title_renderer(context, board->title);
    else if (wena_wekan_link(context, WENA_ICON_NONE, board->title, WENA_WEKAN_FONT_BODY,
                             WENA_WEKAN_HEADER_TEXT))
        action |= WENA_BOARD_HEADER_RENAME;
    /* WeKan's logo after the title, 28 high. */
    if (info != NULL && info->logo != NULL && info->logo_width > 0 && info->logo_height > 0) {
        float logo_w = (float)info->logo_width * 28.0f / (float)info->logo_height;
        nk_layout_space_push(context, nk_rect(buttons_x, 14.0f, logo_w, 28.0f));
        nk_image(context, nk_image_ptr(info->logo));
        buttons_x += logo_w + 24.0f;
    }
    /* The icons the << folds: the desktop icon and the drag handles toggle. */
    if (info != NULL && !info->icons_collapsed && info->drag_handles) {
        nk_layout_space_push(context, nk_rect(buttons_x, 15.0f, 30.0f, 26.0f));
        (void)wena_wekan_icon_button(context, WENA_ICON_DESKTOP, wena_ui_key_text("mobile-desktop-toggle", NULL), 17.0f,
                                     WENA_WEKAN_HEADER_TEXT);
        buttons_x += 30.0f + 18.0f;
        nk_layout_space_push(context, nk_rect(buttons_x, 14.0f, 34.0f, 28.0f));
        if (wena_wekan_icon_button(context, WENA_ICON_ARROWS, wena_ui_key_text("show-desktop-drag-handles", NULL), 12.0f,
                                   WENA_WEKAN_HEADER_LINK))
            action |= WENA_BOARD_HEADER_DRAG_HANDLES;
        /* The check or the ban beside it says which. */
        wena_wekan_icon_draw(context, info->drag_handles == 2 ? WENA_ICON_CHECK : WENA_ICON_BAN, area.x + buttons_x + 17.0f,
                             area.y + 22.0f, 10.0f, WENA_WEKAN_HEADER_LINK);
        buttons_x += 34.0f + 9.0f;
    }

    /* WeKan's star group after the title: caret and count (what the user
     * keeps starred), then the board's star, darker when starred. */
    if (info != NULL && info->star && !info->icons_collapsed) {
        char count[16];
        float x = buttons_x, count_width;
        (void)sprintf(count, "%d", info->starred_count < 0 ? 0 : info->starred_count % 100000);
        count_width = text_width(context, WENA_WEKAN_FONT_SMALL, count) + 22.0f;
        nk_layout_space_push(context, nk_rect(x, 14.0f, count_width, 28.0f));
        if (wena_wekan_link(context, WENA_ICON_CARET_DOWN, count, WENA_WEKAN_FONT_SMALL, WENA_WEKAN_HEADER_TEXT))
            action |= WENA_BOARD_HEADER_STARRED;
        x += count_width + 4.0f;
        if (info->star == 2) wena_wekan_fill(context, area.x + x, area.y + 14.0f, 26.0f, 28.0f, 0x1236D9D, 3.0f);
        nk_layout_space_push(context, nk_rect(x, 14.0f, 26.0f, 28.0f));
        if (wena_wekan_icon_button(context, info->star == 2 ? WENA_ICON_STAR : WENA_ICON_STAR_O,
                                   wena_ui_text(info->star == 2 ? WENA_UI_TEXT_CLICK_TO_UNSTAR :
                                                WENA_UI_TEXT_CLICK_TO_STAR), 13.0f, WENA_WEKAN_HEADER_TEXT))
            action |= WENA_BOARD_HEADER_STAR;
        x += 28.0f;
        if (info->board_stars >= 2) {
            (void)sprintf(count, "%d", info->board_stars % 100000);
            nk_layout_space_push(context, nk_rect(x, 14.0f, text_width(context, WENA_WEKAN_FONT_SMALL, count) + 4.0f, 28.0f));
            wena_wekan_text(context, count, WENA_WEKAN_FONT_SMALL, WENA_WEKAN_HEADER_TEXT, NK_TEXT_LEFT);
            x += text_width(context, WENA_WEKAN_FONT_SMALL, count) + 4.0f;
        }
        buttons_x = x + 12.0f;
    }
    /* WeKan's + that adds a board. */
    if (info != NULL && info->add_board && !info->icons_collapsed) {
        nk_layout_space_push(context, nk_rect(buttons_x, 15.0f, 24.0f, 28.0f));
        if (wena_wekan_icon_button(context, WENA_ICON_PLUS, wena_ui_text(WENA_UI_TEXT_ADD_BOARD), 13.0f,
                                   WENA_WEKAN_HEADER_LINK))
            action |= WENA_BOARD_HEADER_ADD_BOARD;
        buttons_x += 24.0f + 12.0f;
    }
    /* WeKan's Private (or Public) and the user's watch level, each opening
     * its popup. */
    if (info != NULL && info->permission >= 1 && info->permission <= 2) {
        const char *label = wena_ui_text(info->permission == 2 ? WENA_UI_TEXT_PUBLIC : WENA_UI_TEXT_PRIVATE);
        float label_width = text_width(context, WENA_WEKAN_FONT_LINK, label) + 22.0f;
        nk_layout_space_push(context, nk_rect(buttons_x, 14.0f, label_width, 28.0f));
        if (wena_wekan_link(context, info->permission == 2 ? WENA_ICON_GLOBE : WENA_ICON_LOCK, label,
                            WENA_WEKAN_FONT_LINK, WENA_WEKAN_HEADER_LINK))
            action |= WENA_BOARD_HEADER_VISIBILITY;
        buttons_x += label_width + 8.0f;
    }
    if (info != NULL && info->watch >= 1 && info->watch <= 3) {
        static const WenaIcon icons[3] = {WENA_ICON_EYE, WENA_ICON_BELL, WENA_ICON_BELL_SLASH};
        static const WenaUiTextId names[3] = {WENA_UI_TEXT_WATCHING, WENA_UI_TEXT_TRACKING, WENA_UI_TEXT_MUTED};
        const char *label = wena_ui_text(names[info->watch - 1]);
        float label_width = text_width(context, WENA_WEKAN_FONT_LINK, label) + 22.0f;
        nk_layout_space_push(context, nk_rect(buttons_x, 14.0f, label_width, 28.0f));
        if (wena_wekan_link(context, icons[info->watch - 1], label, WENA_WEKAN_FONT_LINK, WENA_WEKAN_HEADER_LINK))
            action |= WENA_BOARD_HEADER_WATCH;
        buttons_x += label_width + 8.0f;
    }
    /* WeKan's Sort Cards; "Sort is on" and the cross that removes it while
     * a sort is on. */
    if (info != NULL && (info->sort == 1 || info->sort == 2)) {
        const char *label = wena_ui_text(info->sort == 2 ? WENA_UI_TEXT_SORT_IS_ON : WENA_UI_TEXT_SORT_CARDS);
        float label_width = text_width(context, WENA_WEKAN_FONT_LINK, label) + 22.0f;
        if (info->sort == 2)
            wena_wekan_fill(context, area.x + buttons_x - 4.0f, area.y + 14.0f, label_width + 8.0f, 28.0f,
                            0x1236D9D, 3.0f);
        nk_layout_space_push(context, nk_rect(buttons_x, 14.0f, label_width, 28.0f));
        if (wena_wekan_link(context, WENA_ICON_SORT, label, WENA_WEKAN_FONT_LINK, WENA_WEKAN_HEADER_LINK))
            action |= WENA_BOARD_HEADER_SORT;
        if (info->sort == 2) {
            nk_layout_space_push(context, nk_rect(buttons_x + label_width + 6.0f, 14.0f, 20.0f, 28.0f));
            if (wena_wekan_icon_button(context, WENA_ICON_TIMES, wena_ui_text(WENA_UI_TEXT_REMOVE_SORT), 11.0f,
                                       WENA_WEKAN_HEADER_TEXT))
                action |= WENA_BOARD_HEADER_SORT_RESET;
        }
    }

    /* Filter, then Search last of the first row's board buttons, as WeKan's;
     * without Search, Filter is last. */
    filter = wena_ui_text(WENA_UI_TEXT_FILTER);
    filter_width = text_width(context, WENA_WEKAN_FONT_LINK, filter) + 22.0f;
    filter_x = width - 81.0f - filter_width;
    if (info != NULL && info->search) {
        const char *search = wena_ui_text(WENA_UI_TEXT_SEARCH);
        float search_width = text_width(context, WENA_WEKAN_FONT_LINK, search) + 22.0f;
        float search_x = width - 17.0f - search_width;
        if (info->search == 2)
            wena_wekan_fill(context, area.x + search_x - 4.0f, area.y + 14.0f, search_width + 8.0f, 28.0f,
                            0x1236D9D, 3.0f);
        nk_layout_space_push(context, nk_rect(search_x, 14.0f, search_width, 28.0f));
        if (wena_wekan_link(context, WENA_ICON_SEARCH, search, WENA_WEKAN_FONT_LINK, WENA_WEKAN_HEADER_LINK))
            action |= WENA_BOARD_HEADER_SEARCH;
        filter_x = search_x - 7.0f - filter_width;
    }
    if (info != NULL && info->filter_active)
        wena_wekan_fill(context, area.x + filter_x - 4.0f, area.y + 14.0f,
                        filter_width + 8.0f, 28.0f, WENA_WEKAN_BUTTON_ADD, 3.0f);
    nk_layout_space_push(context, nk_rect(filter_x, 14.0f, filter_width, 28.0f));
    if (wena_wekan_link(context, WENA_ICON_FILTER, filter, WENA_WEKAN_FONT_LINK, WENA_WEKAN_HEADER_LINK))
        action |= WENA_BOARD_HEADER_FILTER;

    /* Second row, left: Multi-Selection, darker while it is on. */
    if (info != NULL && info->multi_selection) {
        const char *multi = wena_ui_text(WENA_UI_TEXT_MULTI_SELECTION);
        float multi_width = text_width(context, WENA_WEKAN_FONT_LINK, multi) + 22.0f;
        if (info->multi_selection == 2)
            wena_wekan_fill(context, area.x + 12.0f, area.y + 50.0f, multi_width + 8.0f, 28.0f, 0x11A5080, 3.0f);
        nk_layout_space_push(context, nk_rect(16.0f, 50.0f, multi_width, 28.0f));
        if (wena_wekan_link(context, WENA_ICON_CHECK_SQUARE, multi, WENA_WEKAN_FONT_LINK, WENA_WEKAN_HEADER_LINK))
            action |= WENA_BOARD_HEADER_MULTI_SELECTION;
        second_x = 16.0f + multi_width + 8.0f;
    }
    /* WeKan's board view: the caret, the view's icon and its name. */
    if (info != NULL && info->view) {
        const char *view = info->view_name != NULL ? info->view_name :
                           wena_ui_text(info->view == 2 ? WENA_UI_TEXT_BOARD_VIEW_LISTS : WENA_UI_TEXT_BOARD_VIEW_SWIMLANES);
        WenaIcon icon = info->view_name != NULL ? (WenaIcon)info->view_icon : info->view == 2 ? WENA_ICON_LIST : WENA_ICON_GRID;
        float view_width = text_width(context, WENA_WEKAN_FONT_LINK, view) + 22.0f;
        wena_wekan_icon_draw(context, WENA_ICON_CARET_DOWN, area.x + second_x, area.y + 59.0f, 10.0f,
                             WENA_WEKAN_HEADER_LINK);
        nk_layout_space_push(context, nk_rect(second_x + 12.0f, 50.0f, view_width, 28.0f));
        if (wena_wekan_link(context, icon, view, WENA_WEKAN_FONT_LINK, WENA_WEKAN_HEADER_LINK))
            action |= WENA_BOARD_HEADER_VIEW;
        second_x += 12.0f + view_width + 8.0f;
    }
    /* WeKan's Notifications: the bell, white with unread ones, darker while
     * the drawer is open. */
    if (info != NULL && info->notifications >= 1 && info->notifications <= 3) {
        const char *name = wena_ui_text(WENA_UI_TEXT_NOTIFICATIONS);
        float name_width = text_width(context, WENA_WEKAN_FONT_SMALL, name) + 22.0f;
        if (info->notifications == 3)
            wena_wekan_fill(context, area.x + second_x - 4.0f, area.y + 50.0f, name_width + 8.0f, 28.0f,
                            0x1236D9D, 3.0f);
        nk_layout_space_push(context, nk_rect(second_x, 50.0f, name_width, 28.0f));
        if (wena_wekan_link(context, WENA_ICON_BELL, name, WENA_WEKAN_FONT_SMALL,
                            info->notifications == 2 ? WENA_WEKAN_HEADER_TEXT : WENA_WEKAN_HEADER_LINK))
            action |= WENA_BOARD_HEADER_NOTIFICATIONS;
    }

    /* Second row, right: the user's avatar - WeKan's initials in a gray
     * circle - and name, then the sidebar toggle. */
    if (info != NULL && info->actor_name != NULL && info->actor_name[0] != '\0') {
        user_width = text_width(context, WENA_WEKAN_FONT_SMALL, info->actor_name) + 4.0f;
        if (user_width > 200.0f) user_width = 200.0f;
        nk_layout_space_push(context, nk_rect(width - 95.0f - user_width, 52.0f, 24.0f, 24.0f));
        wena_wekan_avatar(context, info->actor_name);
        nk_layout_space_push(context, nk_rect(width - 66.0f - user_width, 50.0f, user_width, 28.0f));
        if (wena_wekan_link(context, WENA_ICON_NONE, info->actor_name, WENA_WEKAN_FONT_SMALL,
                            WENA_WEKAN_HEADER_TEXT))
            action |= WENA_BOARD_HEADER_MEMBER_MENU;
        /* WeKan's separators around the user. */
        wena_wekan_fill(context, area.x + width - 105.0f - user_width, area.y + 54.0f, 1.0f, 20.0f,
                        WENA_WEKAN_HEADER_LINK, 0.0f);
        wena_wekan_fill(context, area.x + width - 56.0f, area.y + 54.0f, 1.0f, 20.0f,
                        WENA_WEKAN_HEADER_LINK, 0.0f);
    }
    (void)sprintf(tooltip, "%.80s %.40s %.80s", wena_ui_text(WENA_UI_TEXT_SIDEBAR_OPEN),
                  wena_ui_text(WENA_UI_TEXT_OR), wena_ui_text(WENA_UI_TEXT_SIDEBAR_CLOSE));
    nk_layout_space_push(context, nk_rect(width - 45.0f, 50.0f, 23.0f, 28.0f));
    if (wena_wekan_icon_button(context, WENA_ICON_BARS, tooltip, 16.0f, WENA_WEKAN_HEADER_TEXT))
        action |= WENA_BOARD_HEADER_OPEN_MENU;
    nk_layout_space_end(context);
    return action;
}
