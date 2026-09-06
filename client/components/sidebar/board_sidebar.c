#include "board_sidebar.h"

#include <nuklear.h>
#include <stddef.h>

void wena_board_sidebar_init(WenaBoardSidebar *sidebar)
{
    if (sidebar != NULL) {
        sidebar->visible = 0;
        sidebar->section = WENA_SIDEBAR_ACTIVITIES;
    }
}

static unsigned int wena_sidebar_section_button(struct nk_context *context,
                                                WenaBoardSidebar *sidebar,
                                                const char *title,
                                                WenaSidebarSection section)
{
    if (nk_button_label(context, title)) {
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
    action = WENA_SIDEBAR_NO_ACTION;
    if (!nk_group_begin(context, "Board menu", NK_WINDOW_BORDER)) {
        return action;
    }
    nk_layout_row_dynamic(context, 28.0f, 2);
    action |= wena_sidebar_section_button(context, sidebar, "Activities",
                                           WENA_SIDEBAR_ACTIVITIES);
    action |= wena_sidebar_section_button(context, sidebar, "Members",
                                           WENA_SIDEBAR_MEMBERS);
    nk_layout_row_dynamic(context, 28.0f, 2);
    action |= wena_sidebar_section_button(context, sidebar, "Labels",
                                           WENA_SIDEBAR_LABELS);
    action |= wena_sidebar_section_button(context, sidebar, "Archives",
                                           WENA_SIDEBAR_ARCHIVES);

    section_title = "Activities";
    if (sidebar->section == WENA_SIDEBAR_MEMBERS) {
        section_title = "Members";
    } else if (sidebar->section == WENA_SIDEBAR_LABELS) {
        section_title = "Labels";
    } else if (sidebar->section == WENA_SIDEBAR_ARCHIVES) {
        section_title = "Archives";
    }
    nk_layout_row_dynamic(context, 28.0f, 1);
    nk_label(context, section_title, NK_TEXT_LEFT);
    if (nk_button_label(context, "Close")) {
        sidebar->visible = 0;
        action |= WENA_SIDEBAR_CLOSED;
    }
    nk_group_end(context);
    return action;
}
