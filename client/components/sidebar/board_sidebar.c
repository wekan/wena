#include "board_sidebar.h"
#include "../../../imports/ui/page_contract.h"

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
static void wena_sidebar_item_list(struct nk_context *context,
    WenaTableState *state, const char *const *items, size_t count,
    const char *empty_text)
{
    WenaTableView view;
    memset(&view, 0, sizeof(view));
    view.row_count = count; view.column_count = 1; view.row_height = 24.0f;
    view.empty_text = empty_text; view.render_row = sidebar_row;
    view.context = (void *)items;
    (void)wena_table_render(context, state, &view);
    nk_layout_row_dynamic(context, 24.0f, 1);
}

static unsigned int wena_sidebar_section_content(struct nk_context *context,
                                                 WenaBoardSidebar *sidebar)
{
    const WenaSidebarItems *items;

    items = &sidebar->items;
    if (sidebar->section == WENA_SIDEBAR_ACTIVITIES) {
        wena_sidebar_item_list(context, &sidebar->table, items->activities, items->activity_count,
                               "No activities");
        return nk_button_label(context, wena_ui_text(WENA_UI_TEXT_REFRESH)) ?
               WENA_SIDEBAR_REFRESH_ACTIVITIES : WENA_SIDEBAR_NO_ACTION;
    }
    if (sidebar->section == WENA_SIDEBAR_MEMBERS) {
        wena_sidebar_item_list(context, &sidebar->table, items->members, items->member_count,
                               "No members");
        return nk_button_label(context, wena_ui_text(WENA_UI_TEXT_ADD_MEMBER)) ?
               WENA_SIDEBAR_ADD_MEMBER : WENA_SIDEBAR_NO_ACTION;
    }
    if (sidebar->section == WENA_SIDEBAR_LABELS) {
        wena_sidebar_item_list(context, &sidebar->table, items->labels, items->label_count,
                               "No labels");
        return nk_button_label(context, wena_ui_text(WENA_UI_TEXT_ADD_LABEL)) ?
               WENA_SIDEBAR_ADD_LABEL : WENA_SIDEBAR_NO_ACTION;
    }
    if (sidebar->section == WENA_SIDEBAR_ARCHIVES) {
        wena_sidebar_item_list(context, &sidebar->table, items->archives, items->archive_count,
                               "No archived items");
        return nk_button_label(context, wena_ui_text(WENA_UI_TEXT_RESTORE)) ?
               WENA_SIDEBAR_RESTORE_ARCHIVE : WENA_SIDEBAR_NO_ACTION;
    }
    return WENA_SIDEBAR_INVALID_STATE;
}

static unsigned int wena_sidebar_section_button(struct nk_context *context,
                                                WenaBoardSidebar *sidebar,
                                                const char *title,
                                                WenaSidebarSection section)
{
    if (nk_button_label(context, title)) {
        if (sidebar->section != section) sidebar->table.page = 0;
        sidebar->section = section;
        return WENA_SIDEBAR_SECTION_CHANGED;
    }
    return WENA_SIDEBAR_NO_ACTION;
}

unsigned int wena_board_sidebar_render(struct nk_context *context,
                                       WenaBoardSidebar *sidebar)
{
    unsigned int action;
    const char *section_title;

    if (context == NULL || sidebar == NULL || !sidebar->visible) {
        return WENA_SIDEBAR_NO_ACTION;
    }
    if (!wena_sidebar_items_valid(&sidebar->items)) {
        return WENA_SIDEBAR_INVALID_STATE;
    }
    action = WENA_SIDEBAR_NO_ACTION;
    if (!nk_group_begin(context, "Board menu", NK_WINDOW_BORDER)) {
        return action;
    }
    nk_layout_row_dynamic(context, 28.0f, 2);
    action |= wena_sidebar_section_button(context, sidebar, wena_ui_text(WENA_UI_TEXT_ACTIVITIES),
                                           WENA_SIDEBAR_ACTIVITIES);
    action |= wena_sidebar_section_button(context, sidebar, wena_ui_text(WENA_UI_TEXT_MEMBERS),
                                           WENA_SIDEBAR_MEMBERS);
    nk_layout_row_dynamic(context, 28.0f, 2);
    action |= wena_sidebar_section_button(context, sidebar, wena_ui_text(WENA_UI_TEXT_LABELS),
                                           WENA_SIDEBAR_LABELS);
    action |= wena_sidebar_section_button(context, sidebar, wena_ui_text(WENA_UI_TEXT_ARCHIVES),
                                           WENA_SIDEBAR_ARCHIVES);

    section_title = wena_ui_text(WENA_UI_TEXT_ACTIVITIES);
    if (sidebar->section == WENA_SIDEBAR_MEMBERS) {
        section_title = wena_ui_text(WENA_UI_TEXT_MEMBERS);
    } else if (sidebar->section == WENA_SIDEBAR_LABELS) {
        section_title = wena_ui_text(WENA_UI_TEXT_LABELS);
    } else if (sidebar->section == WENA_SIDEBAR_ARCHIVES) {
        section_title = wena_ui_text(WENA_UI_TEXT_ARCHIVES);
    }
    nk_layout_row_dynamic(context, 28.0f, 1);
    nk_label(context, section_title, NK_TEXT_LEFT);
    action |= wena_sidebar_section_content(context, sidebar);
    if (nk_button_label(context, "Close")) {
        sidebar->visible = 0;
        action |= WENA_SIDEBAR_CLOSED;
    }
    nk_group_end(context);
    return action;
}
