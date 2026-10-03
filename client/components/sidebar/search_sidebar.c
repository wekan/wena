#include "search_sidebar.h"
#include "../common/wekan_look.h"
#include "../../../imports/ui/page_contract.h"
#include "../../platform/nuklear_options.h"
#include <nuklear.h>
#include <string.h>

void wena_search_sidebar_open(WenaSearchSidebar *search)
{
    if (search == NULL) return;
    search->visible = 1;
    search->focus = 1;
}

static void heading(struct nk_context *context, const char *text)
{
    struct nk_rect line;
    nk_layout_row_dynamic(context, 10.0f, 1);
    line = nk_widget_bounds(context);
    nk_spacing(context, 1);
    wena_wekan_fill(context, line.x, line.y + 4.0f, line.w, 1.0f, WENA_WEKAN_POPUP_BORDER, 0.0f);
    nk_layout_row_dynamic(context, 20.0f, 1);
    wena_wekan_text(context, text, WENA_WEKAN_FONT_BODY, WENA_WEKAN_TEXT, NK_TEXT_LEFT);
}

/* A found list or card, as WeKan's minilist and minicard: white, rounded,
 * the title inside with the minicard's padding. */
static int result(struct nk_context *context, const char *title, int bold)
{
    float x, y, w;
    int clicked;
    nk_layout_space_begin(context, NK_STATIC, 38.0f, 1);
    wena_wekan_space_area(context, 38.0f, &x, &y, &w);
    wena_wekan_fill(context, x, y + 3.0f, w, 32.0f, WENA_WEKAN_MINICARD_SHADOW, 4.0f);
    wena_wekan_fill(context, x, y + 2.0f, w, 32.0f, WENA_WEKAN_MINICARD, 4.0f);
    nk_layout_space_push(context, nk_rect(10.0f, 7.0f, w - 20.0f, 24.0f));
    clicked = wena_wekan_text_button(context, title, bold ? WENA_WEKAN_FONT_BOLD : WENA_WEKAN_FONT_SMALL,
                                     WENA_WEKAN_MINICARD_TEXT);
    nk_layout_space_end(context);
    return clicked;
}

unsigned int wena_search_sidebar_render(struct nk_context *context, WenaSearchSidebar *search,
                                        const WenaList *lists, size_t list_count,
                                        const WenaCard *cards, size_t card_count,
                                        float width, float height, char *card, size_t capacity)
{
    unsigned int action = WENA_SEARCH_NO_ACTION, edit;
    float panel_width, top;
    size_t i, j;
    if (context == NULL || search == NULL || !search->visible || width <= 0.0f || height <= 0.0f)
        return WENA_SEARCH_NO_ACTION;
    if (card != NULL && capacity > 0) card[0] = '\0';
    panel_width = width < 420.0f ? width : 420.0f;
    top = height > 88.0f ? 88.0f : 0.0f;
    nk_style_push_style_item(context, &context->style.window.fixed_background,
        nk_style_item_color(nk_rgb((wena_wekan_rgb(WENA_WEKAN_PANEL) >> 16) & 255,
                                   (wena_wekan_rgb(WENA_WEKAN_PANEL) >> 8) & 255,
                                   wena_wekan_rgb(WENA_WEKAN_PANEL) & 255)));
    if (nk_begin(context, "Wena search", nk_rect(width - panel_width, top, panel_width, height - top), 0)) {
        wena_ui_region("search");
        /* The sidebar's title and its close cross. */
        nk_layout_row_begin(context, NK_DYNAMIC, 30.0f, 2);
        nk_layout_row_push(context, 0.9f);
        wena_wekan_text(context, wena_ui_text(WENA_UI_TEXT_SEARCH), WENA_WEKAN_FONT_SECTION, WENA_WEKAN_TEXT,
                        NK_TEXT_LEFT);
        nk_layout_row_push(context, 0.1f);
        if (wena_wekan_icon_button(context, WENA_ICON_TIMES, wena_ui_text(WENA_UI_TEXT_CLOSE), 14.0f,
                                   WENA_WEKAN_ICON))
            action |= WENA_SEARCH_CLOSE;
        nk_layout_row_end(context);
        nk_layout_row_dynamic(context, 34.0f, 1);
        if (search->focus) { nk_edit_focus(context, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER); search->focus = 0; }
        edit = nk_edit_string(context, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER, search->term, &search->length,
                              (int)sizeof(search->term), nk_filter_default);
        search->term[search->length] = '\0';
        if ((edit & NK_EDIT_COMMITED) != 0u) action |= WENA_SEARCH_SUBMIT;
        if (search->length == 0) {
            /* WeKan's placeholder, until something is written. */
            nk_layout_row_dynamic(context, 18.0f, 1);
            wena_wekan_text(context, wena_ui_text(WENA_UI_TEXT_SEARCH_EXAMPLE), WENA_WEKAN_FONT_SMALL,
                            WENA_WEKAN_ICON, NK_TEXT_LEFT);
        }
        heading(context, wena_ui_text(WENA_UI_TEXT_LISTS));
        for (i = 0; search->searched && i < search->list_count; ++i)
            for (j = 0; j < list_count; ++j)
                if (!strcmp(lists[j].id, search->lists[i])) { (void)result(context, lists[j].title, 1); break; }
        heading(context, wena_ui_text(WENA_UI_TEXT_CARDS));
        for (i = 0; search->searched && i < search->card_count; ++i)
            for (j = 0; j < card_count; ++j)
                if (!strcmp(cards[j].id, search->cards[i])) {
                    if (result(context, cards[j].title, 0) && card != NULL && strlen(cards[j].id) < capacity) {
                        strcpy(card, cards[j].id);
                        action |= WENA_SEARCH_OPEN_CARD;
                    }
                    break;
                }
    }
    nk_end(context);
    nk_style_pop_style_item(context);
    if ((action & WENA_SEARCH_CLOSE) != 0u) search->visible = 0;
    return action;
}
