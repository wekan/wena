#include "card_details_canvas.h"
#include "../common/wekan_look.h"
#include "../../../imports/ui/page_contract.h"

#include "../../platform/nuklear_options.h"
#include <nuklear.h>

/* WeKan's .card-details-header: 56px, the caret at the left, the title, and
 * at the right Card Actions, Maximize and Close Card (cardDetails.jade). */
#define HEADER_ROW 40.0f
#define TITLE_LINE 23.0f

static int icon_slot(struct nk_context *context, float width, WenaIcon icon,
                     const char *name, float size)
{
    nk_layout_row_push(context, width);
    return wena_wekan_icon_button(context, icon, name, size, WENA_WEKAN_ICON);
}

/* cardSectionHeader: the rule above it (not above the first), then the
 * heading that folds the section. */
static int section_header(struct nk_context *context, WenaCardDetailsView *view,
                          WenaCardDetailsSection section, WenaIcon icon, const char *label)
{
    struct nk_rect row;
    if (section != WENA_CARD_DETAILS_SECTION_LABELS) {
        nk_layout_row_dynamic(context, 9.0f, 1);
        row = nk_widget_bounds(context);
        nk_spacer(context); /* one slot: nk_spacing wraps to a new row */
        wena_wekan_fill(context, row.x, row.y + 4.0f, row.w, 1.0f, WENA_WEKAN_POPUP_BORDER, 0.0f);
    }
    nk_layout_row_dynamic(context, 28.0f, 1);
    if (wena_wekan_section_header(context, !view->closed[section], icon, label))
        view->closed[section] = !view->closed[section];
    return !view->closed[section];
}

unsigned int wena_card_details_canvas_render(struct nk_context *context,
                                             const WenaCard *card,
                                             WenaCardDetailsView *view)
{
    unsigned int action;
    struct nk_rect area;
    float title_width;
    int lines;
    if (context == NULL || card == NULL || card->archived || view == NULL ||
        context->current == NULL) {
        return WENA_CARD_DETAILS_NO_ACTION;
    }
    action = WENA_CARD_DETAILS_NO_ACTION;
    wena_ui_region("card-details");

    /* The header: the title wraps, and the row grows with it. */
    nk_layout_row_dynamic(context, 1.0f, 1);
    area = nk_widget_bounds(context);
    nk_spacer(context); /* one slot: nk_spacing wraps to a new row */
    /* The four icons and the spacing between the five slots. */
    title_width = area.w - 34.0f - 35.0f - 44.0f - 38.0f - 4.0f * context->style.window.spacing.x - 2.0f;
    if (title_width < 40.0f) title_width = 40.0f;
    lines = wena_wekan_wrapped_lines(context, card->title, WENA_WEKAN_FONT_TITLE, title_width);
    nk_layout_row_begin(context, NK_STATIC, HEADER_ROW > (float)lines * TITLE_LINE ?
                        HEADER_ROW : (float)lines * TITLE_LINE, 5);
    if (icon_slot(context, 34.0f, view->collapsed ? WENA_ICON_CARET_RIGHT : WENA_ICON_CARET_DOWN,
                  wena_ui_text(WENA_UI_TEXT_COLLAPSE), 14.0f))
        view->collapsed = !view->collapsed;
    nk_layout_row_push(context, title_width);
    if (wena_wekan_text_button(context, card->title, WENA_WEKAN_FONT_TITLE, WENA_WEKAN_TEXT))
        action |= WENA_CARD_DETAILS_EDIT_TITLE;
    if (icon_slot(context, 35.0f, WENA_ICON_BARS, wena_ui_text(WENA_UI_TEXT_CARD_ACTIONS), 16.0f))
        action |= WENA_CARD_DETAILS_OPEN_MENU;
    if (icon_slot(context, 44.0f, view->maximized ? WENA_ICON_WINDOW_MINIMIZE : WENA_ICON_WINDOW_MAXIMIZE,
                  wena_ui_text(view->maximized ? WENA_UI_TEXT_MINIMIZE_CARD : WENA_UI_TEXT_MAXIMIZE_CARD),
                  16.0f))
        view->maximized = !view->maximized;
    if (icon_slot(context, 38.0f, WENA_ICON_TIMES, wena_ui_text(WENA_UI_TEXT_CLOSE_CARD), 16.0f))
        action |= WENA_CARD_DETAILS_CLOSE;
    nk_layout_row_end(context);
    if (view->collapsed) return action;

    /* Labels: the card's chips and "+" to change them. */
    if (section_header(context, view, WENA_CARD_DETAILS_SECTION_LABELS, WENA_ICON_TAG,
                       wena_ui_text(WENA_UI_TEXT_LABELS))) {
        if (view->body != NULL)
            action |= view->body(context, view->body_context, card, WENA_CARD_DETAILS_SECTION_LABELS);
        nk_layout_row_begin(context, NK_STATIC, 28.0f, 1);
        if (icon_slot(context, 28.0f, WENA_ICON_PLUS, wena_ui_text(WENA_UI_TEXT_CARD_LABELS_TITLE), 14.0f))
            action |= WENA_CARD_DETAILS_LABELS;
        nk_layout_row_end(context);
    }
    /* Description: the text, and the pencil that edits it. */
    if (section_header(context, view, WENA_CARD_DETAILS_SECTION_DESCRIPTION, WENA_ICON_ALIGN_LEFT,
                       wena_ui_text(WENA_UI_TEXT_DESCRIPTION))) {
        nk_layout_row_begin(context, NK_STATIC, 24.0f, 1);
        if (icon_slot(context, 24.0f, WENA_ICON_PENCIL, wena_ui_text(WENA_UI_TEXT_EDIT), 14.0f))
            action |= WENA_CARD_DETAILS_DESCRIPTION;
        nk_layout_row_end(context);
        if (view->body != NULL)
            action |= view->body(context, view->body_context, card, WENA_CARD_DETAILS_SECTION_DESCRIPTION);
    }
    /* Checklists: their items, and the editor. */
    if (section_header(context, view, WENA_CARD_DETAILS_SECTION_CHECKLISTS, WENA_ICON_CHECK_SQUARE,
                       wena_ui_text(WENA_UI_TEXT_CHECKLISTS))) {
        if (view->body != NULL)
            action |= view->body(context, view->body_context, card, WENA_CARD_DETAILS_SECTION_CHECKLISTS);
        nk_layout_row_begin(context, NK_STATIC, 28.0f, 1);
        if (icon_slot(context, 28.0f, WENA_ICON_PLUS, wena_ui_control_text(WENA_UI_OPEN_CHECKLISTS), 14.0f))
            action |= WENA_CARD_DETAILS_CHECKLISTS;
        nk_layout_row_end(context);
    }
    return action;
}
