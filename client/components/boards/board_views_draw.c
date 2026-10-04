/* WeKan's board views other than the lists and the report charts, drawn:
 * Table, Calendar, Calendar of every board, Time, Timeline, Stats, Group by
 * Assignee, the three Gantts, Product Backlog, Sprints, Sprint Report,
 * Velocity, Roadmap and Bigboard - each laid out as WeKan's page is, from
 * what models/view_rows.c computes. */
#include "board_views.h"
#include "../../../models/view_rows.h"
#include "../../../models/color.h"
#include "../../../imports/ui/page_contract.h"
#include "../../platform/nuklear_options.h"
#include <nuklear.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DAY_MS 86400000.0
#define ROW 22.0f

static const char *T(const char *key)
{
    return wena_ui_key_text(key, NULL);
}

/* Helpers ------------------------------------------------------------------ */

static void title(struct nk_context *context, const char *text)
{
    nk_layout_row_dynamic(context, 34.0f, 1);
    wena_wekan_text(context, text, WENA_WEKAN_FONT_SECTION, WENA_WEKAN_TEXT, NK_TEXT_LEFT);
}

static void heading(struct nk_context *context, const char *text)
{
    nk_layout_row_dynamic(context, 28.0f, 1);
    wena_wekan_text(context, text, WENA_WEKAN_FONT_BOLD, WENA_WEKAN_TEXT, NK_TEXT_LEFT);
}

static void note(struct nk_context *context, const char *text)
{
    float width = context->current->layout->bounds.w - 24.0f;
    nk_layout_row_dynamic(context, 18.0f * (float)wena_wekan_wrapped_lines(context, text, WENA_WEKAN_FONT_SMALL, width) + 4.0f, 1);
    wena_wekan_text_wrap(context, text, WENA_WEKAN_FONT_SMALL, WENA_WEKAN_ICON);
}

/* A stats-view-table row: the label, its value. */
static void pair(struct nk_context *context, const char *label, const char *value)
{
    nk_layout_row_dynamic(context, ROW, 2);
    wena_wekan_text(context, label, WENA_WEKAN_FONT_SMALL, WENA_WEKAN_ICON, NK_TEXT_LEFT);
    wena_wekan_text(context, value, WENA_WEKAN_FONT_SMALL, WENA_WEKAN_TEXT, NK_TEXT_LEFT);
}

static void cells(struct nk_context *context, int bold, size_t count, const char *const *texts)
{
    size_t i;
    nk_layout_row_dynamic(context, ROW, (int)count);
    for (i = 0; i < count; ++i)
        wena_wekan_text(context, texts[i] != NULL ? texts[i] : "", bold ? WENA_WEKAN_FONT_BOLD : WENA_WEKAN_FONT_SMALL,
                        WENA_WEKAN_TEXT, NK_TEXT_LEFT);
}

static void number_text(double value, char *out, size_t capacity)
{
    wena_chart_number(value, out, capacity);
}

/* A date as WeKan shows it to the user: local time. */
static void date_text(double ms, int with_time, char *out, size_t capacity)
{
    time_t seconds = (time_t)floor(ms / 1000.0);
    struct tm *local = localtime(&seconds);
    if (capacity == 0) return;
    out[0] = '\0';
    if (local != NULL) (void)strftime(out, capacity, with_time ? "%Y-%m-%d %H:%M" : "%Y-%m-%d", local);
}

/* WeKan's label chip: its color, its name. */
static void chip(struct nk_context *context, float x, float y, const char *name, const char *color)
{
    unsigned char rgb[3], fg[3];
    const struct nk_user_font *face = wena_wekan_font(context, WENA_WEKAN_FONT_SMALL);
    struct nk_command_buffer *out = nk_window_get_canvas(context);
    float w;
    if (face == NULL || out == NULL) return;
    w = face->width(face->userdata, face->height, name, (int)strlen(name)) + 10.0f;
    if (!wena_color_rgb(color, rgb)) { rgb[0] = rgb[1] = rgb[2] = 0xcc; }
    if (!wena_color_foreground(color, fg)) { fg[0] = fg[1] = fg[2] = 0x33; }
    nk_fill_rect(out, nk_rect(x, y, w < 18.0f ? 18.0f : w, face->height + 4.0f), 3.0f, nk_rgb(rgb[0], rgb[1], rgb[2]));
    nk_draw_text(out, nk_rect(x + 5.0f, y + 2.0f, w, face->height), name, (int)strlen(name), face, nk_rgba(0, 0, 0, 0),
                 nk_rgb(fg[0], fg[1], fg[2]));
}

static int link(struct nk_context *context, const char *text)
{
    return wena_wekan_link(context, WENA_ICON_NONE, text, WENA_WEKAN_FONT_SMALL, WENA_WEKAN_BUTTON);
}

static int open_card(const WenaViewCard *card, char *out, size_t capacity)
{
    if (card == NULL || out == NULL || strlen(card->id) >= capacity) return 0;
    strcpy(out, card->id);
    return 1;
}

static const char *names(const WenaViewData *data, char ids[][WENA_VIEW_ID], size_t count, char *out, size_t capacity)
{
    size_t i;
    out[0] = '\0';
    for (i = 0; i < count; ++i) {
        const char *name = wena_view_user_name(data, ids[i]);
        if (strlen(out) + strlen(name) + 3 >= capacity) break;
        if (i) strcat(out, ", ");
        strcat(out, name);
    }
    return out;
}

/* Table -------------------------------------------------------------------- */

static const char *const table_keys[WENA_TABLE_FIELD_COUNT] = {
    "card", "list", "swimlane", "assignees", "members", "labels", "card-received", "card-start", "card-due", "card-end"};

static unsigned int table_view(struct nk_context *context, WenaBoardViewState *state, const WenaViewData *data,
                               char *card, size_t capacity)
{
    unsigned int action = WENA_BOARD_VIEW_NO_ACTION;
    size_t count, pages, first, i, f;
    WenaViewTableRow *rows;
    char page[64];
    const char *previous_lane = NULL;
    static const float widths[11] = {0.06f, 0.18f, 0.09f, 0.09f, 0.1f, 0.1f, 0.1f, 0.07f, 0.07f, 0.07f, 0.07f};
    count = wena_view_table_rows(data, state->query, state->sort_field, state->sort_descending, state->group_by_swimlane,
                                 NULL, 0);
    rows = (WenaViewTableRow *)malloc((count + 1) * sizeof(*rows));
    if (rows == NULL) return action;
    (void)wena_view_table_rows(data, state->query, state->sort_field, state->sort_descending, state->group_by_swimlane,
                               rows, count);
    pages = count ? (count + WENA_TABLE_PAGE - 1) / WENA_TABLE_PAGE : 1;
    if (state->page < 1) state->page = 1;
    if ((size_t)state->page > pages) state->page = (int)pages;
    /* The controls: search, the pages, the swimlane grouping. */
    nk_layout_row_begin(context, NK_STATIC, 30.0f, 6);
    nk_layout_row_push(context, 260.0f);
    if ((nk_edit_string(context, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER, state->search, &state->search_length,
                        (int)sizeof(state->search), nk_filter_default) & NK_EDIT_COMMITED) != 0u) {
        state->search[state->search_length] = '\0';
        strcpy(state->query, state->search);
        state->page = 1;
    }
    state->search[state->search_length] = '\0';
    nk_layout_row_push(context, 90.0f);
    if (wena_wekan_link(context, WENA_ICON_SEARCH, T("search"), WENA_WEKAN_FONT_SMALL, WENA_WEKAN_TEXT)) {
        strcpy(state->query, state->search);
        state->page = 1;
    }
    nk_layout_row_push(context, 30.0f);
    if (wena_wekan_button(context, "<", WENA_WEKAN_BUTTON) && state->page > 1) --state->page;
    nk_layout_row_push(context, 60.0f);
    sprintf(page, "%d / %lu", state->page, (unsigned long)pages);
    wena_wekan_text(context, page, WENA_WEKAN_FONT_SMALL, WENA_WEKAN_TEXT, NK_TEXT_CENTERED);
    nk_layout_row_push(context, 30.0f);
    if (wena_wekan_button(context, ">", WENA_WEKAN_BUTTON) && (size_t)state->page < pages) ++state->page;
    nk_layout_row_push(context, 40.0f);
    if (wena_wekan_icon_button(context, state->group_by_swimlane ? WENA_ICON_GRID : WENA_ICON_LIST,
                               T(state->group_by_swimlane ? "board-table-group-by-swimlane-on" :
                                 "board-table-group-by-swimlane-off"), 14.0f, WENA_WEKAN_TEXT))
        state->group_by_swimlane = !state->group_by_swimlane;
    nk_layout_row_end(context);
    /* The header: each column sorts, a second click the other way. */
    nk_layout_row(context, NK_DYNAMIC, 26.0f, 11, widths);
    wena_wekan_text(context, "", WENA_WEKAN_FONT_BOLD, WENA_WEKAN_TEXT, NK_TEXT_LEFT);
    for (f = 0; f < WENA_TABLE_FIELD_COUNT; ++f) {
        const char *label = T(table_keys[f]);
        struct nk_rect cell = nk_widget_bounds(context);
        const struct nk_user_font *face = wena_wekan_font(context, WENA_WEKAN_FONT_BOLD);
        /* The sorted column's caret: down when descending (fa-sort-down). */
        if (state->sort_field == (WenaTableField)f && face != NULL) {
            float x = cell.x + face->width(face->userdata, face->height, label, (int)strlen(label)) + 6.0f;
            if (state->sort_descending) wena_wekan_icon_draw(context, WENA_ICON_CARET_DOWN, x, cell.y + 6.0f, 10.0f, WENA_WEKAN_TEXT);
            else wena_wekan_icon_draw(context, WENA_ICON_ARROW_UP, x, cell.y + 6.0f, 10.0f, WENA_WEKAN_TEXT);
        }
        if (wena_wekan_text_button(context, label, WENA_WEKAN_FONT_BOLD, WENA_WEKAN_TEXT)) {
            if (state->sort_field == (WenaTableField)f) state->sort_descending = !state->sort_descending;
            else { state->sort_field = (WenaTableField)f; state->sort_descending = 0; }
        }
    }
    first = (size_t)(state->page - 1) * WENA_TABLE_PAGE;
    for (i = first; i < count && i < first + WENA_TABLE_PAGE; ++i) {
        const WenaViewCard *c = rows[i].card;
        const WenaViewTime *times[4];
        char text[256], when[32];
        struct nk_rect labels;
        size_t k;
        float x;
        /* addSwimlaneGroupHeaders, on the page's rows. */
        if (state->group_by_swimlane && (previous_lane == NULL || strcmp(previous_lane, rows[i].swimlane_id))) {
            heading(context, rows[i].swimlane_title);
            previous_lane = rows[i].swimlane_id;
        }
        nk_layout_row(context, NK_DYNAMIC, ROW + 4.0f, 11, widths);
        if (link(context, T("edit")) && open_card(c, card, capacity)) action |= WENA_BOARD_VIEW_OPEN_CARD;
        if (wena_wekan_text_button(context, c->title, WENA_WEKAN_FONT_SMALL, WENA_WEKAN_TEXT) && open_card(c, card, capacity))
            action |= WENA_BOARD_VIEW_OPEN_CARD;
        wena_wekan_text(context, rows[i].list_title, WENA_WEKAN_FONT_SMALL, WENA_WEKAN_TEXT, NK_TEXT_LEFT);
        wena_wekan_text(context, rows[i].swimlane_title, WENA_WEKAN_FONT_SMALL, WENA_WEKAN_TEXT, NK_TEXT_LEFT);
        wena_wekan_text(context, names(data, (char (*)[WENA_VIEW_ID])c->assignees, c->assignee_count, text, sizeof(text)),
                        WENA_WEKAN_FONT_SMALL, WENA_WEKAN_TEXT, NK_TEXT_LEFT);
        wena_wekan_text(context, names(data, (char (*)[WENA_VIEW_ID])c->members, c->member_count, text, sizeof(text)),
                        WENA_WEKAN_FONT_SMALL, WENA_WEKAN_TEXT, NK_TEXT_LEFT);
        labels = nk_widget_bounds(context);
        nk_label(context, "", NK_TEXT_LEFT);
        for (k = 0, x = labels.x; k < c->label_count && x < labels.x + labels.w - 20.0f; ++k) {
            const WenaViewLabel *label = wena_view_label(data, c->label_ids[k]);
            const struct nk_user_font *face = wena_wekan_font(context, WENA_WEKAN_FONT_SMALL);
            if (label == NULL || face == NULL) continue;
            chip(context, x, labels.y + 2.0f, label->name, label->color);
            x += face->width(face->userdata, face->height, label->name, (int)strlen(label->name)) + 16.0f;
        }
        times[0] = &c->received_at; times[1] = &c->start_at; times[2] = &c->due_at; times[3] = &c->end_at;
        for (k = 0; k < 4; ++k) {
            when[0] = '\0';
            if (times[k]->set) date_text(times[k]->ms, 0, when, sizeof(when));
            wena_wekan_text(context, when, WENA_WEKAN_FONT_SMALL, WENA_WEKAN_TEXT, NK_TEXT_LEFT);
        }
    }
    if (count == 0) note(context, T("no-cards-found"));
    free(rows);
    return action;
}

/* Calendar ----------------------------------------------------------------- */

static const char *const weekday_keys[7] = {"monday", "tuesday", "wednesday", "thursday", "friday", "saturday", "sunday"};

static long monday_of(long day)
{
    return day - (((day % 7) + 7 + 3) % 7);
}

/* The local day of a time, as the calendar shows it. */
static long local_day(double ms)
{
    time_t seconds = (time_t)floor(ms / 1000.0);
    struct tm *local = localtime(&seconds);
    long offset = 0;
    if (local != NULL) {
        struct tm *utc;
        int lh = local->tm_hour, lm = local->tm_min, lyday = local->tm_yday, lyear = local->tm_year;
        utc = gmtime(&seconds);
        if (utc != NULL) {
            long diff = (long)(lyday - utc->tm_yday) * 1440L + (long)(lh - utc->tm_hour) * 60L + (lm - utc->tm_min);
            if (lyear != utc->tm_year) diff = lyear > utc->tm_year ? diff + 1440L * 365L : diff - 1440L * 365L;
            if (diff > 1440L * 300L) diff -= 1440L * 365L;
            if (diff < -1440L * 300L) diff += 1440L * 365L;
            offset = diff;
        }
    }
    return (long)floor((ms + (double)offset * 60000.0) / DAY_MS);
}

static const char *event_label(const WenaCalendarEvent *e, int all, char *out, size_t capacity)
{
    const char *suffix = e->kind == WENA_CALENDAR_RECEIVED ? T("card-received") : e->kind == WENA_CALENDAR_DUE ?
                         T("card-due") : e->kind == WENA_CALENDAR_END ? T("card-end") : "";
    if (capacity < 300) { out[0] = '\0'; return out; }
    /* The calendar of every board names each card's board. */
    if (all && e->card->board_title[0]) sprintf(out, "%.100s: %.100s%s%.60s", e->card->board_title, e->card->title,
                                                suffix[0] ? " " : "", suffix);
    else sprintf(out, "%.150s%s%.60s", e->card->title, suffix[0] ? " " : "", suffix);
    return out;
}

static unsigned int calendar_view(struct nk_context *context, WenaBoardViewState *state, const WenaViewData *data,
                                  int all, double now, char *card, size_t capacity)
{
    unsigned int action = WENA_BOARD_VIEW_NO_ACTION;
    long today = local_day(now), first, last, day, month_first;
    WenaCalendarEvent *events;
    size_t count, i;
    char head[64], key[11], label[320];
    static const char *const modes[4] = {"day", "week", "month", "list"};
    int m;
    /* WeKan opens on the month (initialView dayGridMonth), at today. */
    if (state->calendar_day == 0) { state->calendar_day = today; state->calendar_mode = 2; }
    wena_chart_day_key(state->calendar_day, key);
    month_first = state->calendar_day - (atol(key + 8) - 1);
    if (state->calendar_mode == 2 || state->calendar_mode == 3) {
        first = state->calendar_mode == 2 ? monday_of(month_first) : month_first;
        last = month_first + 31;
        wena_chart_day_key(last, key);
        last -= atol(key + 8);    /* the month's last day */
        if (state->calendar_mode == 2) last = monday_of(last) + 6;
        memcpy(head, key, 7); head[7] = '\0';
        wena_chart_day_key(month_first, key);
        memcpy(head, key, 7); head[7] = '\0';
    } else if (state->calendar_mode == 1) {
        first = monday_of(state->calendar_day);
        last = first + 6;
        wena_chart_day_key(first, key);
        sprintf(head, "%s \xe2\x80\x93 ", key);
        wena_chart_day_key(last, key);
        strcat(head, key);
    } else {
        first = last = state->calendar_day;
        wena_chart_day_key(first, head);
    }
    /* The toolbar: the title at the left; Today, Previous, Next and the
     * views at the right, as WeKan's headerToolbar. */
    nk_layout_row_begin(context, NK_DYNAMIC, 32.0f, 8);
    nk_layout_row_push(context, 0.34f);
    wena_wekan_text(context, head, WENA_WEKAN_FONT_SECTION, WENA_WEKAN_TEXT, NK_TEXT_LEFT);
    nk_layout_row_push(context, 0.1f);
    if (wena_wekan_button(context, T("today"), WENA_WEKAN_BUTTON)) state->calendar_day = today;
    nk_layout_row_push(context, 0.09f);
    if (wena_wekan_button(context, T("previous"), WENA_WEKAN_BUTTON))
        state->calendar_day -= state->calendar_mode >= 2 ? 30 : state->calendar_mode == 1 ? 7 : 1;
    nk_layout_row_push(context, 0.09f);
    if (wena_wekan_button(context, T("next"), WENA_WEKAN_BUTTON))
        state->calendar_day += state->calendar_mode >= 2 ? 31 : state->calendar_mode == 1 ? 7 : 1;
    for (m = 0; m < 4; ++m) {
        nk_layout_row_push(context, 0.09f);
        if (wena_wekan_button(context, T(modes[m]), state->calendar_mode == m ? WENA_WEKAN_BUTTON_ADD : WENA_WEKAN_BUTTON))
            state->calendar_mode = m;
    }
    nk_layout_row_end(context);
    events = (WenaCalendarEvent *)malloc((data->card_count * 4 + 1) * sizeof(*events));
    if (events == NULL) return action;
    count = wena_view_calendar_events(data, (double)first * DAY_MS - 14.0 * 3600000.0,
                                      (double)(last + 1) * DAY_MS + 14.0 * 3600000.0, events, data->card_count * 4);
    if (state->calendar_mode == 2) {
        /* The month: a week a row, Monday first. */
        long weeks = (last - first + 1) / 7, w;
        int d;
        nk_layout_row_dynamic(context, 22.0f, 7);
        for (d = 0; d < 7; ++d) wena_wekan_text(context, T(weekday_keys[d]), WENA_WEKAN_FONT_BOLD, WENA_WEKAN_TEXT, NK_TEXT_CENTERED);
        for (w = 0; w < weeks; ++w) {
            nk_layout_row_dynamic(context, 96.0f, 7);
            for (d = 0; d < 7; ++d) {
                struct nk_rect cell;
                long this_day = first + w * 7 + d;
                int shown = 0, more = 0;
                char number[8], group[32];
                day = this_day;
                sprintf(group, "calendar/%ld", this_day);
                cell = nk_widget_bounds(context);
                wena_wekan_fill(context, cell.x, cell.y, cell.w, cell.h, this_day == today ? 0x1fcf8e3 : 0x1ffffff, 0.0f);
                wena_wekan_fill(context, cell.x, cell.y, cell.w, 1.0f, WENA_WEKAN_LIST_BORDER, 0.0f);
                nk_style_push_style_item(context, &context->style.window.fixed_background,
                                         nk_style_item_color(nk_rgba(0, 0, 0, 0)));
                nk_style_push_float(context, &context->style.window.group_border, 0.0f);
                if (nk_group_begin_titled(context, group, "", NK_WINDOW_NO_SCROLLBAR)) {
                    nk_style_pop_float(context);
                    nk_style_pop_style_item(context);
                    wena_chart_day_key(day, key);
                    sprintf(number, "%ld", atol(key + 8));
                    nk_layout_row_dynamic(context, 16.0f, 1);
                    wena_wekan_text(context, number, WENA_WEKAN_FONT_SMALL, day >= month_first && day < month_first + 31 &&
                                    !strncmp(key, head, 7) ? WENA_WEKAN_TEXT : WENA_WEKAN_ICON, NK_TEXT_RIGHT);
                    for (i = 0; i < count; ++i) {
                        if (local_day(events[i].start) > day || local_day(events[i].end - 1.0) < day) continue;
                        if (shown == 3) { ++more; continue; }
                        nk_layout_row_dynamic(context, 16.0f, 1);
                        {
                            struct nk_rect chip_area = nk_widget_bounds(context);
                            wena_wekan_fill(context, chip_area.x, chip_area.y, chip_area.w, chip_area.h, 0x13788d8, 3.0f);
                        }
                        if (wena_wekan_text_button(context, event_label(&events[i], all, label, sizeof(label)),
                                                   WENA_WEKAN_FONT_SMALL, WENA_WEKAN_BUTTON_TEXT) &&
                            open_card(events[i].card, card, capacity)) action |= WENA_BOARD_VIEW_OPEN_CARD;
                        ++shown;
                    }
                    if (more) {
                        char text[32];
                        sprintf(text, "+%d", more);
                        nk_layout_row_dynamic(context, 14.0f, 1);
                        wena_wekan_text(context, text, WENA_WEKAN_FONT_SMALL, WENA_WEKAN_ICON, NK_TEXT_LEFT);
                    }
                    nk_group_end(context);
                } else {
                    nk_style_pop_float(context);
                    nk_style_pop_style_item(context);
                }
            }
        }
    } else {
        /* A day, a week, or the month as a list: each day with its events. */
        for (day = first; day <= last; ++day) {
            int any = 0;
            for (i = 0; i < count; ++i) {
                char when[32];
                if (local_day(events[i].start) > day || local_day(events[i].end - 1.0) < day) continue;
                if (!any) {
                    char dayhead[96];
                    long monday = monday_of(day);
                    wena_chart_day_key(day, key);
                    sprintf(dayhead, "%s %.40s", key, T(weekday_keys[day - monday]));
                    heading(context, dayhead);
                    any = 1;
                }
                nk_layout_row_begin(context, NK_DYNAMIC, ROW, 2);
                nk_layout_row_push(context, 0.18f);
                date_text(events[i].start, 1, when, sizeof(when));
                wena_wekan_text(context, when + 11, WENA_WEKAN_FONT_SMALL, WENA_WEKAN_ICON, NK_TEXT_LEFT);
                nk_layout_row_push(context, 0.8f);
                if (wena_wekan_text_button(context, event_label(&events[i], all, label, sizeof(label)), WENA_WEKAN_FONT_SMALL,
                                           WENA_WEKAN_BUTTON) && open_card(events[i].card, card, capacity))
                    action |= WENA_BOARD_VIEW_OPEN_CARD;
                nk_layout_row_end(context);
            }
        }
        if (count == 0) note(context, T("no-cards-found"));
    }
    free(events);
    return action;
}

/* Stats, Time, Group by Assignee ------------------------------------------- */

static void stats_view(struct nk_context *context, const WenaViewData *data)
{
    size_t i;
    long cards = 0, archived = 0;
    char value[32];
    for (i = 0; i < data->card_count; ++i) {
        if (data->cards[i].deleted_at.set) continue;
        if (data->cards[i].archived) ++archived; else ++cards;
    }
    title(context, data->board_title);
    heading(context, T("board-status"));
    pair(context, T("board-status-loading-mode"), T("cards-loading-all"));
    sprintf(value, "%lu", (unsigned long)data->swimlane_count); pair(context, T("swimlanes"), value);
    sprintf(value, "%lu", (unsigned long)data->list_count); pair(context, T("lists"), value);
    sprintf(value, "%ld", cards); pair(context, T("cards"), value);
    sprintf(value, "%ld", archived); pair(context, T("card-archived"), value);
    sprintf(value, "%lu", (unsigned long)data->label_count); pair(context, T("labels"), value);
    sprintf(value, "%d", data->active_members); pair(context, T("members"), value);
    sprintf(value, "%lu", (unsigned long)data->custom_field_count); pair(context, T("custom-fields"), value);
}

static void time_view(struct nk_context *context, const WenaViewData *data, double now)
{
    WenaTimeSummary summary;
    char value[128], hours[32];
    size_t i;
    const char *head[5];
    title(context, T("board-view-time"));
    if (!wena_view_time(data, now, T("no-assignee"), &summary)) return;
    heading(context, T("board-status-time-summary"));
    number_text(summary.spent_total, hours, sizeof(hours));
    sprintf(value, "%s h", hours);
    pair(context, T("board-status-time-spent-total"), value);
    sprintf(value, "%ld", summary.cards_with_time); pair(context, T("board-status-cards-with-time"), value);
    sprintf(value, "%ld", summary.overtime_cards); pair(context, T("board-status-overtime-cards"), value);
    /* formatRemainingTime: one minus on the whole duration. */
    sprintf(value, "%s%ld %.30s, %ld %.30s", summary.remaining_days < 0 || summary.remaining_hour < 0 ? "-" : "",
            labs(summary.remaining_days), T("days"), labs(summary.remaining_hour), T("hours"));
    pair(context, T("board-status-remaining-time-total"), value);
    if (summary.assignee_count || summary.card_count) {
        heading(context, T("assignees"));
        head[0] = T("name"); head[1] = T("hours");
        cells(context, 1, 2, head);
        for (i = 0; i < summary.assignee_count; ++i) {
            number_text(summary.by_assignee[i].hours, hours, sizeof(hours));
            head[0] = summary.by_assignee[i].label; head[1] = hours;
            cells(context, 0, 2, head);
        }
        heading(context, T("card"));
        head[0] = T("name"); head[1] = T("hours");
        cells(context, 1, 2, head);
        for (i = 0; i < summary.card_count; ++i) {
            number_text(summary.by_card[i]->spent_time, hours, sizeof(hours));
            sprintf(value, "%.90s%s%.30s%s", summary.by_card[i]->title, summary.by_card[i]->is_overtime ? " (" : "",
                    summary.by_card[i]->is_overtime ? T("overtime") : "", summary.by_card[i]->is_overtime ? ")" : "");
            head[0] = value; head[1] = hours;
            cells(context, 0, 2, head);
        }
    }
    heading(context, T("time-adjustments"));
    note(context, T("time-adjustment-note"));
    head[0] = T("username"); head[1] = T("hours");
    cells(context, 1, 2, head);
    for (i = 0; i < summary.adjustment_count; ++i) {
        number_text(summary.adjustments[i].hours, hours, sizeof(hours));
        head[0] = summary.adjustments[i].label; head[1] = hours;
        cells(context, 0, 2, head);
    }
    head[0] = T("date"); head[1] = T("card"); head[2] = T("username"); head[3] = "\xce\x94"; head[4] = T("hours");
    cells(context, 1, 5, head);
    for (i = 0; i < summary.entry_count; ++i) {
        char when[32], delta[32], total[32];
        const WenaViewCard *c = wena_view_card(data, summary.entries[i].card_id);
        date_text(summary.entries[i].at, 1, when, sizeof(when));
        number_text(summary.entries[i].hours, delta, sizeof(delta));
        number_text(summary.entries[i].total, total, sizeof(total));
        head[0] = when; head[1] = c != NULL ? c->title : summary.entries[i].card_id;
        head[2] = wena_view_user_name(data, summary.entries[i].user_id); head[3] = delta; head[4] = total;
        cells(context, 0, 5, head);
    }
    wena_view_time_free(&summary);
}

static unsigned int assignee_view(struct nk_context *context, const WenaViewData *data, char *card, size_t capacity)
{
    unsigned int action = WENA_BOARD_VIEW_NO_ACTION;
    WenaAssigneeGroup *groups = NULL;
    long count, g;
    size_t i;
    title(context, T("board-view-group-by-assignee"));
    count = wena_view_assignee_groups(data, T("no-assignee"), &groups);
    if (count <= 0) { note(context, T("group-by-assignee-empty")); wena_view_groups_free(groups, count); return action; }
    for (g = 0; g < count; ++g) {
        char head[200];
        sprintf(head, "%.150s (%lu)", groups[g].label, (unsigned long)groups[g].card_count);
        heading(context, head);
        for (i = 0; i < groups[g].card_count; ++i) {
            const WenaViewCard *c = groups[g].cards[i];
            char due[64];
            nk_layout_row_begin(context, NK_DYNAMIC, ROW, 3);
            nk_layout_row_push(context, 0.6f);
            if (wena_wekan_text_button(context, c->title, WENA_WEKAN_FONT_SMALL, WENA_WEKAN_BUTTON) &&
                open_card(c, card, capacity)) action |= WENA_BOARD_VIEW_OPEN_CARD;
            nk_layout_row_push(context, 0.3f);
            due[0] = '\0';
            if (c->due_at.set) date_text(c->due_at.ms, 1, due, sizeof(due));
            wena_wekan_text(context, due, WENA_WEKAN_FONT_SMALL, WENA_WEKAN_ICON, NK_TEXT_LEFT);
            nk_layout_row_push(context, 0.08f);
            if (c->is_overtime) (void)wena_wekan_icon_button(context, WENA_ICON_CLOCK, T("overtime"), 12.0f, WENA_WEKAN_WIP_EXCEEDED);
            else wena_wekan_text(context, "", WENA_WEKAN_FONT_SMALL, WENA_WEKAN_TEXT, NK_TEXT_LEFT);
            nk_layout_row_end(context);
        }
    }
    wena_view_groups_free(groups, count);
    return action;
}

/* Timeline ----------------------------------------------------------------- */

static unsigned int timeline_view(struct nk_context *context, WenaBoardViewState *state, const WenaViewData *data,
                                  float height)
{
    double markers[50];
    size_t marker_count = wena_view_timeline_markers(data, markers, 50), i, l, count;
    WenaTimelineCard *cards;
    char when[32];
    title(context, T("board-view-timeline"));
    note(context, T("board-view-timeline-hint"));
    /* The markers: Now, then the times, a row of five. */
    nk_layout_row_dynamic(context, 26.0f, 6);
    if (wena_wekan_button(context, T("board-view-timeline-now"), state->has_selected_time ? WENA_WEKAN_BUTTON : WENA_WEKAN_BUTTON_ADD))
        state->has_selected_time = 0;
    for (i = 0; i < marker_count; ++i) {
        if ((i + 1) % 6 == 0) nk_layout_row_dynamic(context, 26.0f, 6);
        date_text(markers[i], 1, when, sizeof(when));
        if (wena_wekan_button(context, when, state->has_selected_time && state->selected_time == markers[i] ?
                              WENA_WEKAN_BUTTON_ADD : WENA_WEKAN_BUTTON)) {
            state->has_selected_time = 1;
            state->selected_time = markers[i];
        }
    }
    if (state->has_selected_time) {
        char text[160];
        date_text(state->selected_time, 1, when, sizeof(when));
        sprintf(text, "%.100s %s", T("board-view-timeline-showing"), when);
        note(context, text);
    }
    cards = (WenaTimelineCard *)malloc((data->card_count + 1) * sizeof(*cards));
    if (cards == NULL) return WENA_BOARD_VIEW_NO_ACTION;
    count = wena_view_timeline(data, !state->has_selected_time, state->selected_time, cards);
    /* The lists side by side, each with the cards as they were. */
    if (data->list_count > 0) {
        float column = 260.0f;
        nk_layout_row_begin(context, NK_STATIC, height > 200.0f ? height - 160.0f : 300.0f, (int)data->list_count);
        for (l = 0; l < data->list_count; ++l) {
            char id[96];
            nk_layout_row_push(context, column);
            sprintf(id, "timeline/%.60s", data->lists[l].id);
            if (nk_group_begin_titled(context, id, "", 0)) {
                heading(context, data->lists[l].title);
                for (i = 0; i < count; ++i) {
                    const WenaTimelineCard *c = &cards[i];
                    size_t k;
                    char line[300];
                    if (!c->existed || strcmp(c->list_id, data->lists[l].id)) continue;
                    if (c->archived) { nk_layout_row_dynamic(context, 16.0f, 1);
                        wena_wekan_text(context, T("archived"), WENA_WEKAN_FONT_SMALL, WENA_WEKAN_WIP_EXCEEDED, NK_TEXT_LEFT); }
                    nk_layout_row_dynamic(context, ROW, 1);
                    wena_wekan_text(context, c->title, WENA_WEKAN_FONT_BOLD, WENA_WEKAN_TEXT, NK_TEXT_LEFT);
                    if (c->description[0]) note(context, c->description);
                    line[0] = '\0';
                    for (k = 0; k < c->label_count; ++k) {
                        const WenaViewLabel *label = wena_view_label(data, c->label_ids[k]);
                        const char *name = label == NULL ? "" : label->name[0] ? label->name : label->color;
                        if (strlen(line) + strlen(name) + 3 < sizeof(line)) { if (line[0]) strcat(line, ", "); strcat(line, name); }
                    }
                    if (line[0]) { nk_layout_row_dynamic(context, 16.0f, 1);
                        wena_wekan_text(context, line, WENA_WEKAN_FONT_SMALL, WENA_WEKAN_ICON, NK_TEXT_LEFT); }
                    if (c->member_count) {
                        char members[200];
                        sprintf(line, "%.40s: %.200s", T("members"), names(data, (char (*)[WENA_VIEW_ID])c->members,
                                                                           c->member_count, members, sizeof(members)));
                        nk_layout_row_dynamic(context, 16.0f, 1);
                        wena_wekan_text(context, line, WENA_WEKAN_FONT_SMALL, WENA_WEKAN_ICON, NK_TEXT_LEFT);
                    }
                    if (c->due_at.set) {
                        date_text(c->due_at.ms, 1, when, sizeof(when));
                        sprintf(line, "%.60s: %s", T("due-date"), when);
                        nk_layout_row_dynamic(context, 16.0f, 1);
                        wena_wekan_text(context, line, WENA_WEKAN_FONT_SMALL, WENA_WEKAN_ICON, NK_TEXT_LEFT);
                    }
                    nk_layout_row_dynamic(context, 6.0f, 1);
                    nk_label(context, "", NK_TEXT_LEFT);
                }
                nk_group_end(context);
            }
        }
        nk_layout_row_end(context);
    }
    free(cards);
    return WENA_BOARD_VIEW_NO_ACTION;
}

/* Gantt -------------------------------------------------------------------- */

static unsigned int gantt_view(struct nk_context *context, const WenaViewData *data, double now, char *card,
                               size_t capacity)
{
    unsigned int action = WENA_BOARD_VIEW_NO_ACTION;
    long weeks[520];
    size_t week_count = wena_view_gantt_weeks(data, weeks, 520), w, i;
    long today = local_day(now);
    title(context, T("board-view-gantt"));
    for (w = 0; w < week_count; ++w) {
        long year;
        int week, d;
        char head[96];
        wena_view_iso_week(weeks[w], &year, &week);
        nk_layout_row_dynamic(context, 34.0f, 8);
        sprintf(head, "%.30s %.20s %d", T("task"), T("predicate-week"), week);
        wena_wekan_text(context, head, WENA_WEKAN_FONT_BOLD, WENA_WEKAN_TEXT, NK_TEXT_LEFT);
        for (d = 0; d < 7; ++d) {
            char key[11], text[80];
            wena_chart_day_key(weeks[w] + d, key);
            sprintf(text, "%s %.30s", key, T(weekday_keys[d]));
            wena_wekan_text(context, text, WENA_WEKAN_FONT_SMALL, WENA_WEKAN_TEXT, NK_TEXT_LEFT);
        }
        for (i = 0; i < data->card_count; ++i) {
            const WenaViewCard *c = &data->cards[i];
            const WenaViewTime *times[4];
            int k, any = 0;
            static const int colors[4] = {0x1dbdbdb, 0x190ee90, 0x1ffd700, 0x1ffb3b3};
            static const WenaIcon icons[4] = {WENA_ICON_ARCHIVE, WENA_ICON_ARROW_UP, WENA_ICON_CLOCK, WENA_ICON_CHECK};
            if (c->deleted_at.set) continue;
            times[0] = &c->received_at; times[1] = &c->start_at; times[2] = &c->due_at; times[3] = &c->end_at;
            for (k = 0; k < 4; ++k) if (times[k]->set && local_day(times[k]->ms) >= weeks[w] && local_day(times[k]->ms) < weeks[w] + 7) any = 1;
            if (!any) continue;
            nk_layout_row_dynamic(context, ROW + 2.0f, 8);
            if (wena_wekan_text_button(context, c->title, WENA_WEKAN_FONT_SMALL, WENA_WEKAN_BUTTON) && open_card(c, card, capacity))
                action |= WENA_BOARD_VIEW_OPEN_CARD;
            for (d = 0; d < 7; ++d) {
                struct nk_rect cell = nk_widget_bounds(context);
                long this_day = weeks[w] + d;
                int found = -1;
                for (k = 0; k < 4 && found < 0; ++k) if (times[k]->set && local_day(times[k]->ms) == this_day) found = k;
                if (found >= 0) {
                    wena_wekan_fill(context, cell.x, cell.y, cell.w, cell.h, colors[found], 0.0f);
                    wena_wekan_icon_draw(context, icons[found], cell.x + cell.w / 2.0f - 6.0f, cell.y + 5.0f, 12.0f, WENA_WEKAN_TEXT);
                } else if (this_day == today) wena_wekan_fill(context, cell.x, cell.y, cell.w, cell.h, 0x1fcf8e3, 0.0f);
                else if (d >= 5) wena_wekan_fill(context, cell.x, cell.y, cell.w, cell.h, 0x1f0f0f0, 0.0f);
                nk_label(context, "", NK_TEXT_LEFT);
            }
        }
    }
    if (week_count == 0) note(context, T("no-cards-found"));
    return action;
}

/* A timeline of bars, as Frappe Gantt and DHTMLX Gantt draw theirs: `scale`
 * days a column, a bar per task from its start to its end, red when
 * overdue, filled when done; `grid` adds DHTMLX's Task name / Start time /
 * Duration columns at the left. */
static unsigned int bars(struct nk_context *context, const WenaGanttTask *tasks, size_t count, int scale, int grid,
                         double now, char *card, size_t capacity)
{
    unsigned int action = WENA_BOARD_VIEW_NO_ACTION;
    double first = 0.0, last = 0.0;
    long columns, c;
    size_t i;
    float left, column_width = scale >= 30 ? 90.0f : scale >= 7 ? 70.0f : 32.0f, row_height = 28.0f, top, width;
    struct nk_rect area;
    struct nk_command_buffer *out;
    for (i = 0; i < count; ++i) {
        if (i == 0 || tasks[i].start < first) first = tasks[i].start;
        if (i == 0 || tasks[i].end > last) last = tasks[i].end;
    }
    if (count == 0) return action;
    first -= (double)scale * DAY_MS;
    if (scale == 7) first = (double)monday_of(wena_chart_day(first)) * DAY_MS;
    columns = (long)ceil((last - first) / ((double)scale * DAY_MS)) + 2;
    if (columns > 400) columns = 400;
    left = grid ? 420.0f : 0.0f;
    width = left + column_width * (float)columns;
    nk_layout_row_static(context, 40.0f + row_height * (float)count + 8.0f, (int)width, 1);
    area = nk_widget_bounds(context);
    nk_label(context, "", NK_TEXT_LEFT);
    out = nk_window_get_canvas(context);
    if (out == NULL) return action;
    wena_wekan_fill(context, area.x, area.y, area.w, area.h, 0x1ffffff, 0.0f);
    top = area.y + 40.0f;
    /* The scale: each column's first day; today's line. */
    for (c = 0; c < columns; ++c) {
        char key[11];
        float x = area.x + left + column_width * (float)c;
        nk_stroke_line(out, x, area.y, x, area.y + area.h, 1.0f, nk_rgb(0xe0, 0xe0, 0xe0));
        wena_chart_day_key(wena_chart_day(first) + c * scale, key);
        {
            const struct nk_user_font *face = wena_wekan_font(context, WENA_WEKAN_FONT_SMALL);
            if (face != NULL)
                nk_draw_text(out, nk_rect(x + 3.0f, area.y + 12.0f, column_width - 4.0f, face->height),
                             scale >= 30 ? key : key + 5, scale >= 30 ? 7 : 5, face, nk_rgba(0, 0, 0, 0), nk_rgb(0x55, 0x55, 0x55));
        }
    }
    {
        float x = area.x + left + (float)((floor(now / DAY_MS) * DAY_MS - first) / ((double)scale * DAY_MS)) * column_width;
        if (x > area.x + left && x < area.x + area.w) nk_stroke_line(out, x, area.y, x, area.y + area.h, 2.0f, nk_rgb(0xfc, 0xa1, 0x4c));
    }
    if (grid) {
        const struct nk_user_font *face = wena_wekan_font(context, WENA_WEKAN_FONT_BOLD);
        const char *heads[3];
        int h;
        heads[0] = T("task"); heads[1] = T("card-start"); heads[2] = T("duration");
        for (h = 0; h < 3 && face != NULL; ++h)
            nk_draw_text(out, nk_rect(area.x + 6.0f + (float)h * 140.0f, area.y + 12.0f, 136.0f, face->height),
                         heads[h], (int)strlen(heads[h]), face, nk_rgba(0, 0, 0, 0), nk_rgb(0x33, 0x33, 0x33));
        nk_stroke_line(out, area.x + left, area.y, area.x + left, area.y + area.h, 1.0f, nk_rgb(0xbb, 0xbb, 0xbb));
    }
    for (i = 0; i < count; ++i) {
        float y = top + row_height * (float)i;
        float x0 = area.x + left + (float)((tasks[i].start - first) / ((double)scale * DAY_MS)) * column_width;
        float x1 = area.x + left + (float)((tasks[i].end - first) / ((double)scale * DAY_MS)) * column_width;
        int color = tasks[i].overdue ? 0x1e74c3c : 0x1a3a3ff;
        const struct nk_user_font *face = wena_wekan_font(context, WENA_WEKAN_FONT_SMALL);
        if (x1 - x0 < 6.0f) x1 = x0 + 6.0f;
        wena_wekan_fill(context, x0, y + 4.0f, x1 - x0, row_height - 8.0f, color, 3.0f);
        if (tasks[i].done) wena_wekan_fill(context, x0, y + 4.0f, x1 - x0, row_height - 8.0f, tasks[i].overdue ? 0x1c0392b : 0x15a5aff, 3.0f);
        if (face != NULL) {
            const char *name = tasks[i].card->title;
            nk_draw_text(out, nk_rect(grid ? area.x + 6.0f : x1 + 6.0f, y + 6.0f, grid ? 134.0f : 300.0f, face->height),
                         name, (int)strlen(name), face, nk_rgba(0, 0, 0, 0), nk_rgb(0x33, 0x33, 0x33));
            if (grid) {
                char start[16], days[16];
                wena_chart_day_key(wena_chart_day(tasks[i].start), start);
                sprintf(days, "%ld", (long)floor((tasks[i].end - tasks[i].start) / DAY_MS + 0.5));
                nk_draw_text(out, nk_rect(area.x + 146.0f, y + 6.0f, 130.0f, face->height), start, (int)strlen(start), face,
                             nk_rgba(0, 0, 0, 0), nk_rgb(0x33, 0x33, 0x33));
                nk_draw_text(out, nk_rect(area.x + 286.0f, y + 6.0f, 130.0f, face->height), days, (int)strlen(days), face,
                             nk_rgba(0, 0, 0, 0), nk_rgb(0x33, 0x33, 0x33));
            }
        }
        /* A bar opens its card, as WeKan's click opens it. */
        if (nk_input_mouse_clicked(&context->input, NK_BUTTON_LEFT, nk_rect(x0, y + 4.0f, x1 - x0, row_height - 8.0f)) &&
            open_card(tasks[i].card, card, capacity))
            action |= WENA_BOARD_VIEW_OPEN_CARD;
    }
    return action;
}

static unsigned int frappe_view(struct nk_context *context, WenaBoardViewState *state, const WenaViewData *data, int grid,
                                double now, char *card, size_t capacity)
{
    unsigned int action;
    const WenaViewCard **cards = (const WenaViewCard **)malloc((data->card_count + 1) * sizeof(*cards));
    WenaGanttTask *tasks = (WenaGanttTask *)malloc((data->card_count + 1) * sizeof(*tasks));
    size_t i, n = 0, count;
    static const char *const modes[3] = {"day", "week", "month"};
    static const int scales[3] = {1, 7, 30};
    int m;
    title(context, T(grid ? "board-view-gantt-dhtmlx" : "board-view-gantt-frappe"));
    if (cards == NULL || tasks == NULL) { free((void *)cards); free(tasks); return WENA_BOARD_VIEW_NO_ACTION; }
    for (i = 0; i < data->card_count; ++i) if (!data->cards[i].deleted_at.set) cards[n++] = &data->cards[i];
    count = wena_view_gantt_tasks(cards, n, now, tasks);
    /* The view modes, Week first as WeKan opens them. */
    if (state->gantt_mode < 0 || state->gantt_mode > 2) state->gantt_mode = 1;
    nk_layout_row_static(context, 28.0f, 90, 3);
    for (m = 0; m < 3; ++m)
        if (wena_wekan_button(context, T(modes[m]), state->gantt_mode == m ? WENA_WEKAN_BUTTON_ADD : WENA_WEKAN_BUTTON))
            state->gantt_mode = m;
    if (count == 0) note(context, T("no-cards-found"));
    action = bars(context, tasks, count, scales[state->gantt_mode], grid, now, card, capacity);
    free((void *)cards);
    free(tasks);
    return action;
}

static unsigned int roadmap_view(struct nk_context *context, WenaBoardViewState *state, const WenaViewData *data, double now,
                                 char *card, size_t capacity)
{
    unsigned int action = WENA_BOARD_VIEW_NO_ACTION;
    size_t i, usable = 0;
    title(context, T("board-view-roadmap"));
    for (i = 0; i < data->custom_field_count; ++i)
        if (!strcmp(data->custom_fields[i].type, "text") || !strcmp(data->custom_fields[i].type, "dropdown")) ++usable;
    if (usable == 0) { note(context, T("roadmap-empty-no-custom-fields")); return action; }
    /* The field to group by: the first by name, as WeKan picks it. */
    nk_layout_row_dynamic(context, 28.0f, (int)(usable + 1 > 6 ? 6 : usable + 1));
    wena_wekan_text(context, T("roadmap-group-by"), WENA_WEKAN_FONT_SMALL, WENA_WEKAN_TEXT, NK_TEXT_LEFT);
    for (i = 0; i < data->custom_field_count; ++i) {
        const WenaViewCustomField *f = &data->custom_fields[i];
        if (strcmp(f->type, "text") && strcmp(f->type, "dropdown")) continue;
        if (!state->field_id[0]) strcpy(state->field_id, f->id);
        if (wena_wekan_button(context, f->name, strcmp(state->field_id, f->id) ? WENA_WEKAN_BUTTON : WENA_WEKAN_BUTTON_ADD))
            strcpy(state->field_id, f->id);
    }
    {
        WenaAssigneeGroup *groups = NULL;
        long count = wena_view_roadmap_groups(data, state->field_id, T("roadmap-no-value"), &groups), g;
        WenaGanttTask *tasks = (WenaGanttTask *)malloc((data->card_count + 1) * sizeof(*tasks));
        if (count <= 0 || tasks == NULL) note(context, T("roadmap-no-cards"));
        for (g = 0; tasks != NULL && g < count; ++g) {
            char head[200];
            size_t n;
            sprintf(head, "%.150s (%lu)", groups[g].label, (unsigned long)groups[g].card_count);
            heading(context, head);
            n = wena_view_gantt_tasks(groups[g].cards, groups[g].card_count, now, tasks);
            action |= bars(context, tasks, n, 7, 0, now, card, capacity);
        }
        free(tasks);
        wena_view_groups_free(groups, count);
    }
    return action;
}

/* Scrum -------------------------------------------------------------------- */

static const char *sprint_name(const WenaViewData *data, const char *id)
{
    size_t i;
    for (i = 0; i < data->sprint_count; ++i) if (!strcmp(data->sprints[i].id, id)) return data->sprints[i].name;
    return T("scrum-product-backlog");
}

static const char *release_name(const WenaViewData *data, const char *id)
{
    size_t i;
    for (i = 0; i < data->release_count; ++i) if (!strcmp(data->releases[i].id, id)) return data->releases[i].name;
    return "\xe2\x80\x94";
}

static const char *state_label(const char *state)
{
    char key[48];
    sprintf(key, "scrum-state-%.30s", state);
    return wena_ui_key_text(key, state);
}

static unsigned int backlog_table(struct nk_context *context, const WenaViewData *data, const char *sprint_id,
                                  char *card, size_t capacity)
{
    unsigned int action = WENA_BOARD_VIEW_NO_ACTION;
    const WenaViewCard **cards = (const WenaViewCard **)malloc((data->card_count + 1) * sizeof(*cards));
    size_t count, i;
    const char *head[5];
    if (cards == NULL) return action;
    count = wena_view_scrum_cards(data, sprint_id, cards);
    heading(context, T("scrum-backlog"));
    note(context, T("scrum-backlog-help"));
    head[0] = T("card"); head[1] = T("scrum-estimate"); head[2] = T("scrum-sprint"); head[3] = T("scrum-backlog-rank");
    head[4] = T("scrum-release");
    cells(context, 1, 5, head);
    for (i = 0; i < count; ++i) {
        const WenaViewCard *c = cards[i];
        char estimate[32], rank[32];
        double value;
        if (wena_view_card_estimate(data, c, &value)) number_text(value, estimate, sizeof(estimate));
        else strcpy(estimate, "");
        if (c->backlog_rank.set) number_text(c->backlog_rank.ms, rank, sizeof(rank)); else strcpy(rank, "\xe2\x80\x94");
        nk_layout_row_dynamic(context, ROW, 5);
        if (wena_wekan_text_button(context, c->title, WENA_WEKAN_FONT_SMALL, WENA_WEKAN_BUTTON) && open_card(c, card, capacity))
            action |= WENA_BOARD_VIEW_OPEN_CARD;
        wena_wekan_text(context, estimate[0] ? estimate : T("scrum-unknown-estimate"), WENA_WEKAN_FONT_SMALL, WENA_WEKAN_TEXT, NK_TEXT_LEFT);
        wena_wekan_text(context, sprint_name(data, c->sprint_id), WENA_WEKAN_FONT_SMALL, WENA_WEKAN_TEXT, NK_TEXT_LEFT);
        wena_wekan_text(context, rank, WENA_WEKAN_FONT_SMALL, WENA_WEKAN_TEXT, NK_TEXT_LEFT);
        wena_wekan_text(context, release_name(data, c->release_id), WENA_WEKAN_FONT_SMALL, WENA_WEKAN_TEXT, NK_TEXT_LEFT);
    }
    if (count == 0) note(context, T("no-cards-found"));
    free((void *)cards);
    return action;
}

/* scrumReportTable: the sprints' totals; a total is "count / estimate". */
static void report_table(struct nk_context *context, const WenaViewSprint *const *sprints, size_t count)
{
    const char *head[8];
    size_t i;
    int t;
    head[0] = T("scrum-sprint"); head[1] = T("scrum-estimate-unit"); head[2] = T("scrum-committed");
    head[3] = T("scrum-completed"); head[4] = T("scrum-added"); head[5] = T("scrum-removed");
    head[6] = T("scrum-incomplete"); head[7] = T("scrum-working-days");
    cells(context, 1, 8, head);
    for (i = 0; i < count; ++i) {
        char totals[5][64], days[16];
        for (t = 0; t < 5; ++t) {
            char c[24], e[24];
            number_text(sprints[i]->totals[t][0], c, sizeof(c));
            number_text(sprints[i]->totals[t][1], e, sizeof(e));
            sprintf(totals[t], "%s / %s", c, e);
            head[2 + t] = totals[t];
        }
        if (sprints[i]->working_days >= 0) sprintf(days, "%d", sprints[i]->working_days); else strcpy(days, "\xe2\x80\x94");
        head[0] = sprints[i]->name; head[1] = sprints[i]->unit; head[7] = days;
        cells(context, 0, 8, head);
    }
    if (count == 0) note(context, T("scrum-no-closed-sprints"));
    note(context, T("scrum-report-help"));
}

static int sprint_picker(struct nk_context *context, WenaBoardViewState *state, const WenaViewData *data)
{
    size_t i;
    nk_layout_row_dynamic(context, 28.0f, 5);
    wena_wekan_text(context, T("scrum-sprint"), WENA_WEKAN_FONT_SMALL, WENA_WEKAN_TEXT, NK_TEXT_LEFT);
    for (i = 0; i < data->sprint_count; ++i) {
        char name[200];
        sprintf(name, "%.120s (%.40s)", data->sprints[i].name, state_label(data->sprints[i].state));
        if (wena_wekan_button(context, name, strcmp(state->sprint_id, data->sprints[i].id) ? WENA_WEKAN_BUTTON : WENA_WEKAN_BUTTON_ADD))
            strcpy(state->sprint_id, data->sprints[i].id);
    }
    for (i = 0; i < data->sprint_count; ++i) if (!strcmp(state->sprint_id, data->sprints[i].id)) return (int)i;
    return -1;
}

static unsigned int scrum_view(struct nk_context *context, WenaBoardViewState *state, const WenaViewData *data,
                               const char *key, char *card, size_t capacity)
{
    unsigned int action = WENA_BOARD_VIEW_NO_ACTION;
    size_t i;
    title(context, T(key + 0));
    if (!strcmp(key, "board-view-velocity")) {
        const WenaViewSprint **sprints = (const WenaViewSprint **)malloc((data->sprint_count + 1) * sizeof(*sprints));
        size_t count;
        if (sprints == NULL) return action;
        count = wena_view_velocity(data, sprints);
        report_table(context, sprints, count);
        free((void *)sprints);
    } else if (!strcmp(key, "board-view-sprint-report")) {
        int selected = sprint_picker(context, state, data);
        if (selected < 0) note(context, T("scrum-select-sprint"));
        else {
            const WenaViewSprint *sprint = &data->sprints[selected];
            report_table(context, &sprint, sprint->has_report ? 1 : 0);
        }
    } else if (!strcmp(key, "board-view-sprints")) {
        int selected;
        heading(context, T("scrum-sprints"));
        selected = sprint_picker(context, state, data);
        if (selected >= 0) {
            note(context, data->sprints[selected].goal);
            note(context, state_label(data->sprints[selected].state));
            action |= backlog_table(context, data, data->sprints[selected].id, card, capacity);
            heading(context, T("scrum-events"));
            for (i = 0; i < data->event_count; ++i) {
                char when[32], line[400], kind[48];
                if (strcmp(data->events[i].sprint_id, data->sprints[selected].id)) continue;
                sprintf(kind, "scrum-event-%.30s", data->events[i].kind);
                date_text(data->events[i].starts_at.ms, 1, when, sizeof(when));
                sprintf(line, "%.120s \xe2\x80\x94 %.60s \xe2\x80\x94 %s", data->events[i].name, wena_ui_key_text(kind, data->events[i].kind), when);
                nk_layout_row_dynamic(context, ROW, 1);
                wena_wekan_text(context, line, WENA_WEKAN_FONT_SMALL, WENA_WEKAN_TEXT, NK_TEXT_LEFT);
                if (data->events[i].notes[0]) note(context, data->events[i].notes);
            }
        } else action |= backlog_table(context, data, "", card, capacity);
        heading(context, T("scrum-releases"));
        for (i = 0; i < data->release_count; ++i) {
            const WenaViewRelease *r = &data->releases[i];
            char line[400], start[32], end[32];
            sprintf(line, "%.150s \xe2\x80\x94 %.60s", r->name, state_label(r->state));
            heading(context, line);
            if (r->goal[0]) note(context, r->goal);
            start[0] = end[0] = '\0';
            if (r->planned_start.set) date_text(r->planned_start.ms, 0, start, sizeof(start));
            if (r->planned_end.set) date_text(r->planned_end.ms, 0, end, sizeof(end));
            sprintf(line, "%s \xe2\x80\x94 %s", start, end);
            note(context, line);
            if (r->released_at.set) {
                date_text(r->released_at.ms, 1, start, sizeof(start));
                sprintf(line, "%.60s: %s", T("scrum-released-at"), start);
                note(context, line);
            }
            if (r->notes[0]) note(context, r->notes);
        }
    } else action |= backlog_table(context, data, "", card, capacity);
    return action;
}

/* Bigboard ----------------------------------------------------------------- */

static unsigned int bigboard_view(struct nk_context *context, const WenaViewData *all, char *card, size_t capacity)
{
    unsigned int action = WENA_BOARD_VIEW_NO_ACTION;
    size_t b, l, i;
    if (all == NULL || all->board_count == 0) { note(context, T("no-boards-selected")); return action; }
    for (b = 0; b < all->board_count; ++b) {
        size_t lists = 0;
        title(context, all->boards[b].title);
        for (l = 0; l < all->list_count; ++l) if (!strcmp(all->lists[l].board_id, all->boards[b].id)) ++lists;
        if (lists == 0) continue;
        nk_layout_row_begin(context, NK_STATIC, 260.0f, (int)lists);
        for (l = 0; l < all->list_count; ++l) {
            char id[96];
            if (strcmp(all->lists[l].board_id, all->boards[b].id)) continue;
            nk_layout_row_push(context, 220.0f);
            sprintf(id, "bigboard/%.60s", all->lists[l].id);
            nk_style_push_style_item(context, &context->style.window.fixed_background,
                                     nk_style_item_color(nk_rgb(0xe4, 0xe4, 0xe4)));
            if (nk_group_begin_titled(context, id, "", 0)) {
                heading(context, all->lists[l].title);
                for (i = 0; i < all->card_count; ++i) {
                    const WenaViewCard *c = &all->cards[i];
                    struct nk_rect area;
                    if (strcmp(c->list_id, all->lists[l].id)) continue;
                    nk_layout_row_dynamic(context, 30.0f, 1);
                    area = nk_widget_bounds(context);
                    wena_wekan_fill(context, area.x, area.y + 2.0f, area.w, area.h - 4.0f, WENA_WEKAN_MINICARD, 4.0f);
                    if (wena_wekan_text_button(context, c->title, WENA_WEKAN_FONT_SMALL, WENA_WEKAN_MINICARD_TEXT) &&
                        open_card(c, card, capacity)) action |= WENA_BOARD_VIEW_OPEN_CARD;
                }
                nk_group_end(context);
            }
            nk_style_pop_style_item(context);
        }
        nk_layout_row_end(context);
    }
    return action;
}

/* The entry ------------------------------------------------------------------ */

unsigned int wena_board_view_render(struct nk_context *context, size_t index, WenaBoardViewState *state,
                                    const WenaViewData *data, const WenaViewData *all, double now, float height,
                                    char *card, size_t capacity)
{
    size_t count;
    const WenaBoardView *views = wena_board_views(&count);
    const char *key;
    if (context == NULL || context->current == NULL || state == NULL || data == NULL || index >= count)
        return WENA_BOARD_VIEW_NO_ACTION;
    if (card != NULL && capacity > 0) card[0] = '\0';
    key = views[index].key;
    if (!strcmp(key, "board-view-table")) return table_view(context, state, data, card, capacity);
    if (!strcmp(key, "board-view-cal")) return calendar_view(context, state, data, 0, now, card, capacity);
    if (!strcmp(key, "board-view-multiboard-cal")) return all != NULL ? calendar_view(context, state, all, 1, now, card, capacity) : 0u;
    if (!strcmp(key, "board-view-time")) { time_view(context, data, now); return WENA_BOARD_VIEW_NO_ACTION; }
    if (!strcmp(key, "board-view-timeline")) return timeline_view(context, state, data, height);
    if (!strcmp(key, "board-view-stats")) { stats_view(context, data); return WENA_BOARD_VIEW_NO_ACTION; }
    if (!strcmp(key, "board-view-group-by-assignee")) return assignee_view(context, data, card, capacity);
    if (!strcmp(key, "board-view-gantt")) return gantt_view(context, data, now, card, capacity);
    if (!strcmp(key, "board-view-gantt-frappe")) return frappe_view(context, state, data, 0, now, card, capacity);
    if (!strcmp(key, "board-view-gantt-dhtmlx")) return frappe_view(context, state, data, 1, now, card, capacity);
    if (!strcmp(key, "board-view-roadmap")) return roadmap_view(context, state, data, now, card, capacity);
    if (!strcmp(key, "board-view-bigboard")) return bigboard_view(context, all, card, capacity);
    if (!strncmp(key, "board-view-product-backlog", 26) || !strcmp(key, "board-view-sprints") ||
        !strcmp(key, "board-view-sprint-report") || !strcmp(key, "board-view-velocity"))
        return scrum_view(context, state, data, key, card, capacity);
    return WENA_BOARD_VIEW_NO_ACTION;
}
