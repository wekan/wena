#include "notifications_drawer.h"
#include "../common/wekan_look.h"
#include "../../../imports/ui/page_contract.h"
#include "../../platform/nuklear_options.h"
#include <nuklear.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

size_t wena_notifications_unread(const WenaWekanNotification *items, size_t count)
{
    size_t i, unread = 0;
    for (i = 0; items != NULL && i < count; ++i) unread += !items[i].read;
    return unread;
}

void wena_notifications_text(const WenaWekanNotification *item, char *out, size_t capacity)
{
    if (out == NULL || capacity == 0) return;
    out[0] = '\0';
    if (item == NULL) return;
    if (item->user[0] && item->title[0] && capacity > strlen(item->user) + strlen(item->title) + 3)
    {
        size_t user = strlen(item->user), title = strlen(item->title);
        memcpy(out, item->user, user);
        memcpy(out + user, " - ", 3);
        memcpy(out + user + 3, item->title, title + 1);
    }
    else if (strlen(item->user[0] ? item->user : item->title) < capacity)
        strcpy(out, item->user[0] ? item->user : item->title);
}

unsigned int wena_notifications_drawer_render(struct nk_context *context, const WenaWekanNotification *items,
                                              size_t count, float width, float height, int *index)
{
    unsigned int action = WENA_NOTIFICATIONS_NO_ACTION;
    float panel_width, top;
    char title[160], line[300], when[32];
    size_t i, unread;
    if (context == NULL || width <= 0.0f || height <= 0.0f || (items == NULL && count > 0))
        return WENA_NOTIFICATIONS_NO_ACTION;
    if (index != NULL) *index = -1;
    panel_width = width < 420.0f ? width : 420.0f;
    top = height > 88.0f ? 88.0f : 0.0f;
    unread = wena_notifications_unread(items, count);
    nk_style_push_style_item(context, &context->style.window.fixed_background,
                             nk_style_item_color(nk_rgb(0xfa, 0xfa, 0xfa)));
    if (nk_begin(context, "Wena notifications", nk_rect(width - panel_width, top, panel_width, height - top), 0)) {
        wena_ui_region("notifications");
        nk_layout_row_begin(context, NK_DYNAMIC, 30.0f, 2);
        nk_layout_row_push(context, 0.9f);
        if (unread > 0) sprintf(title, "%.120s (%lu)", wena_ui_text(WENA_UI_TEXT_NOTIFICATIONS), (unsigned long)unread);
        else sprintf(title, "%.120s", wena_ui_text(WENA_UI_TEXT_NOTIFICATIONS));
        wena_wekan_text(context, title, WENA_WEKAN_FONT_SECTION, WENA_WEKAN_TEXT, NK_TEXT_LEFT);
        nk_layout_row_push(context, 0.1f);
        if (wena_wekan_icon_button(context, WENA_ICON_TIMES, wena_ui_text(WENA_UI_TEXT_CLOSE), 14.0f,
                                   WENA_WEKAN_ICON))
            action |= WENA_NOTIFICATIONS_CLOSE;
        nk_layout_row_end(context);
        if (unread > 0) {
            nk_layout_row_dynamic(context, 24.0f, 1);
            if (wena_wekan_link(context, WENA_ICON_CHECK, wena_ui_text(WENA_UI_TEXT_MARK_ALL_READ),
                                WENA_WEKAN_FONT_SMALL, WENA_WEKAN_TEXT))
                action |= WENA_NOTIFICATIONS_MARK_ALL_READ;
        }
        for (i = 0; i < count; ++i) {
            time_t seconds = (time_t)(items[i].at / 1000);
            struct tm *local = localtime(&seconds);
            wena_notifications_text(&items[i], line, sizeof(line));
            nk_layout_row_dynamic(context, 26.0f, 1);
            if (wena_wekan_checkbox(context, items[i].read, line, 1) && index != NULL) {
                *index = items[i].index;
                action |= WENA_NOTIFICATIONS_TOGGLE_READ;
            }
            when[0] = '\0';
            if (local != NULL) (void)strftime(when, sizeof(when), "%Y-%m-%d %H:%M", local);
            nk_layout_row_dynamic(context, 16.0f, 1);
            wena_wekan_text(context, when, WENA_WEKAN_FONT_SMALL, WENA_WEKAN_ICON, NK_TEXT_LEFT);
        }
    }
    nk_end(context);
    nk_style_pop_style_item(context);
    return action;
}
