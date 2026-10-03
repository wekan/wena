#include "board_sidebar.h"
#include "../common/wekan_look.h"
#include "../../../models/color.h"
#include "../../../imports/ui/page_contract.h"

#include "../../platform/nuklear_options.h"
#include <nuklear.h>
#include <stddef.h>
#include <string.h>

void wena_board_sidebar_init(WenaBoardSidebar *sidebar)
{
    if (sidebar != NULL) {
        memset(sidebar, 0, sizeof(*sidebar));
        sidebar->section = WENA_SIDEBAR_ACTIVITIES;
        (void)wena_table_init(&sidebar->table, 10);
    }
}

static int wena_sidebar_items_valid(const WenaSidebarItems *items)
{
    return !((items->activity_count != 0 && items->activities == NULL) ||
             (items->member_count != 0 && items->members == NULL) ||
             (items->label_count != 0 && items->labels == NULL) ||
             (items->archive_count != 0 && items->archives == NULL));
}

static unsigned int sidebar_row(struct nk_context *context, void *data, size_t row)
{
    const char *const *items;
    items = (const char *const *)data;
    nk_label(context, items[row] ? items[row] : "", NK_TEXT_LEFT);
    return 0;
}
/* A section: the rule above it, its heading (which folds it), its items. */
static int fold_header(struct nk_context *context, WenaBoardSidebar *sidebar, WenaSidebarFold fold,
                       WenaIcon icon, const char *label)
{
    struct nk_rect row;
    nk_layout_row_dynamic(context, 9.0f, 1);
    row = nk_widget_bounds(context);
    nk_spacer(context);
    wena_wekan_fill(context, row.x, row.y + 4.0f, row.w, 1.0f, WENA_WEKAN_POPUP_BORDER, 0.0f);
    nk_layout_row_dynamic(context, 28.0f, 1);
    if (wena_wekan_section_header(context, !sidebar->folded[fold], icon, label))
        sidebar->folded[fold] = !sidebar->folded[fold];
    return !sidebar->folded[fold];
}

static void plain_items(struct nk_context *context, const char *const *items, size_t count,
                        WenaIcon icon, const char *empty_text)
{
    size_t index;
    if (count == 0) {
        nk_layout_row_dynamic(context, 24.0f, 1);
        wena_wekan_text(context, empty_text, WENA_WEKAN_FONT_SMALL, WENA_WEKAN_ICON, NK_TEXT_LEFT);
    }
    for (index = 0; index < count; ++index) {
        nk_layout_row_dynamic(context, 24.0f, 1);
        if (icon != WENA_ICON_NONE)
            (void)wena_wekan_link(context, icon, items[index] ? items[index] : "", WENA_WEKAN_FONT_BODY,
                                  WENA_WEKAN_TEXT);
        else wena_wekan_text(context, items[index] ? items[index] : "", WENA_WEKAN_FONT_BODY,
                             WENA_WEKAN_TEXT, NK_TEXT_LEFT);
    }
}

/* The board's labels as WeKan's .card-label chips: bold text on the label's
 * color, 4px rounded, as many to a row as fit. */
static void label_chips(struct nk_context *context, const WenaSidebarItems *items)
{
    const struct nk_user_font *face = wena_wekan_font(context, WENA_WEKAN_FONT_BOLD);
    struct nk_rect row, chip;
    unsigned char rgb[3], text[3];
    size_t index;
    float x, width;
    nk_layout_row_dynamic(context, 1.0f, 1);
    row = nk_widget_bounds(context);
    nk_spacer(context);
    x = row.w;
    for (index = 0; index < items->label_count; ++index) {
        const char *name = items->labels[index] != NULL ? items->labels[index] : "";
        width = (face != NULL ? face->width(face->userdata, face->height, name, (int)strlen(name)) : 40.0f) + 16.0f;
        if (width > row.w) width = row.w;
        if (x + width > row.w) {
            nk_layout_row_dynamic(context, 26.0f, 1);
            chip = nk_widget_bounds(context);
            nk_spacer(context);
            chip.x = row.x; x = 0.0f;
        }
        chip.x = row.x + x; chip.w = width; chip.h = 22.0f;
        if (!wena_color_rgb(items->label_colors[index], rgb) ||
            !wena_color_foreground(items->label_colors[index], text)) {
            rgb[0] = rgb[1] = rgb[2] = 0xa6; text[0] = text[1] = text[2] = 0xff;
        }
        wena_wekan_fill(context, chip.x, chip.y, chip.w, chip.h,
                        0x1000000 | (rgb[0] << 16) | (rgb[1] << 8) | rgb[2], 4.0f);
        wena_ui_control_record(NULL, name, chip.x, chip.y, chip.w, chip.h);
        if (face != NULL)
            nk_draw_text(nk_window_get_canvas(context), nk_rect(chip.x + 8.0f, chip.y + (chip.h - face->height) / 2.0f,
                         chip.w - 16.0f, face->height), name, (int)strlen(name), face, nk_rgba(0, 0, 0, 0),
                         nk_rgb(text[0], text[1], text[2]));
        x += width + 4.0f;
    }
}

/* "+" after a section's items, as WeKan's .add-member and .add-label. */
static int plus(struct nk_context *context, const char *name)
{
    nk_layout_row_begin(context, NK_STATIC, 28.0f, 1);
    nk_layout_row_push(context, 28.0f);
    {
        int clicked = wena_wekan_icon_button(context, WENA_ICON_PLUS, name, 14.0f, WENA_WEKAN_ICON);
        nk_layout_row_end(context);
        return clicked;
    }
}

static unsigned int choose(WenaBoardSidebar *sidebar, WenaSidebarSection section)
{
    if (sidebar->section != section) sidebar->table.page = 0;
    sidebar->section = section;
    return WENA_SIDEBAR_SECTION_CHANGED;
}

unsigned int wena_board_sidebar_render(struct nk_context *context,
                                       WenaBoardSidebar *sidebar)
{
    unsigned int action;
    const WenaSidebarItems *items;
    WenaTableView view;
    struct nk_rect row;

    if (context == NULL || sidebar == NULL || !sidebar->visible) {
        return WENA_SIDEBAR_NO_ACTION;
    }
    if (!wena_sidebar_items_valid(&sidebar->items) || (int)sidebar->section < 0 ||
        sidebar->section > WENA_SIDEBAR_SETTINGS) {
        return WENA_SIDEBAR_INVALID_STATE;
    }
    items = &sidebar->items;
    action = WENA_SIDEBAR_NO_ACTION;
    if (!nk_group_begin(context, "Board menu", 0)) {
        return action;
    }
    wena_ui_region("sidebar");
    /* WeKan's .sidebar-actions: the cross that closes it, at the right. */
    nk_layout_row_dynamic(context, 1.0f, 1);
    row = nk_widget_bounds(context);
    nk_spacer(context);
    nk_layout_row_begin(context, NK_STATIC, 32.0f, 2);
    nk_layout_row_push(context, row.w - 32.0f - context->style.window.spacing.x - 1.0f > 0.0f ?
                       row.w - 32.0f - context->style.window.spacing.x - 1.0f : 0.0f);
    nk_spacer(context);
    nk_layout_row_push(context, 32.0f);
    if (wena_wekan_icon_button(context, WENA_ICON_TIMES, wena_ui_text(WENA_UI_TEXT_CLOSE), 16.0f,
                               WENA_WEKAN_TEXT)) {
        sidebar->visible = 0;
        action |= WENA_SIDEBAR_CLOSED;
    }
    nk_layout_row_end(context);
    /* membersWidget: Board Settings first. */
    nk_layout_row_dynamic(context, 28.0f, 1);
    if (wena_wekan_link(context, WENA_ICON_GEAR, wena_ui_text(WENA_UI_TEXT_BOARD_SETTINGS),
                        WENA_WEKAN_FONT_BOLD, WENA_WEKAN_ICON_ACTIVE))
        action |= choose(sidebar, WENA_SIDEBAR_SETTINGS);
    if (fold_header(context, sidebar, WENA_SIDEBAR_FOLD_MEMBERS, WENA_ICON_USERS,
                    wena_ui_text(WENA_UI_TEXT_MEMBERS))) {
        plain_items(context, items->members, items->member_count, WENA_ICON_USER, "No members");
        if (plus(context, wena_ui_text(WENA_UI_TEXT_ADD_MEMBER))) action |= WENA_SIDEBAR_ADD_MEMBER;
    }
    /* labelsWidget: its "+" opens the board's labels. */
    if (fold_header(context, sidebar, WENA_SIDEBAR_FOLD_LABELS, WENA_ICON_TAG,
                    wena_ui_text(WENA_UI_TEXT_LABELS))) {
        if (items->label_colors != NULL && items->label_count > 0) label_chips(context, items);
        else plain_items(context, items->labels, items->label_count, WENA_ICON_NONE, "No labels");
        if (plus(context, wena_ui_text(WENA_UI_TEXT_ADD_LABEL)))
            action |= WENA_SIDEBAR_ADD_LABEL | choose(sidebar, WENA_SIDEBAR_LABELS);
    }
    /* The board's archive, which WeKan keeps in its sidebar menu. */
    nk_layout_row_dynamic(context, 9.0f, 1);
    nk_spacer(context);
    nk_layout_row_dynamic(context, 28.0f, 1);
    if (wena_wekan_link(context, WENA_ICON_ARCHIVE, wena_ui_text(WENA_UI_TEXT_ARCHIVES),
                        WENA_WEKAN_FONT_BODY, WENA_WEKAN_TEXT))
        action |= choose(sidebar, WENA_SIDEBAR_ARCHIVES);
    if (sidebar->section == WENA_SIDEBAR_ARCHIVES) {
        plain_items(context, items->archives, items->archive_count, WENA_ICON_NONE, "No archived items");
        nk_layout_row_dynamic(context, 28.0f, 1);
        if (wena_wekan_button(context, wena_ui_text(WENA_UI_TEXT_RESTORE), WENA_WEKAN_BUTTON_ADD))
            action |= WENA_SIDEBAR_RESTORE_ARCHIVE;
    }
    /* Activities last, as in WeKan, a page at a time. */
    if (fold_header(context, sidebar, WENA_SIDEBAR_FOLD_ACTIVITIES, WENA_ICON_HISTORY,
                    wena_ui_text(WENA_UI_TEXT_ACTIVITIES))) {
        memset(&view, 0, sizeof(view));
        view.row_count = items->activity_count; view.column_count = 1; view.row_height = 24.0f;
        view.empty_text = "No activities"; view.render_row = sidebar_row;
        view.context = (void *)items->activities;
        (void)wena_table_render(context, &sidebar->table, &view);
        nk_layout_row_begin(context, NK_STATIC, 28.0f, 1);
        nk_layout_row_push(context, 28.0f);
        if (wena_wekan_icon_button(context, WENA_ICON_REFRESH, wena_ui_text(WENA_UI_TEXT_REFRESH), 14.0f,
                                   WENA_WEKAN_ICON))
            action |= WENA_SIDEBAR_REFRESH_ACTIVITIES;
        nk_layout_row_end(context);
    }
    nk_group_end(context);
    return action;
}
