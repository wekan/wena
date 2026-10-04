#include "member_settings.h"
#include "../../../imports/ui/page_contract.h"

#include "../../platform/nuklear_options.h"
#include <nuklear.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* memberMenuPopup, as userHeader.jade lists it: the icon and the key. */
static const struct {
    WenaMemberItem item;
    WenaIcon icon;
    const char *key;
    int enabled;
    int separator_before;
} entries[] = {
    {WENA_MEMBER_MY_CARDS, WENA_ICON_LIST, "my-cards", 0, 0},
    {WENA_MEMBER_DUE_CARDS, WENA_ICON_CALENDAR, "dueCards-title", 0, 0},
    {WENA_MEMBER_MY_ATTACHMENTS, WENA_ICON_PAPERCLIP, "my-attachments", 0, 0},
    {WENA_MEMBER_STARRED, WENA_ICON_STAR, "allboards.starred", 0, 0},
    {WENA_MEMBER_GLOBAL_SEARCH, WENA_ICON_SEARCH, "globalSearch-title", 0, 0},
    {WENA_MEMBER_ALL_BOARDS, WENA_ICON_HOME, "all-boards", 1, 0},
    {WENA_MEMBER_PUBLIC, WENA_ICON_GLOBE, "public", 0, 0},
    {WENA_MEMBER_ARCHIVES, WENA_ICON_ARCHIVE, "archives", 0, 0},
    {WENA_MEMBER_TEMPLATES, WENA_ICON_LIST, "templates", 0, 1},
    {WENA_MEMBER_ADMIN_PANEL, WENA_ICON_LOCK, "admin-panel", 1, 0},
    {WENA_MEMBER_EDIT_PROFILE, WENA_ICON_USER, "edit-profile", 1, 1},
    {WENA_MEMBER_DATE, WENA_ICON_CALENDAR, "date", 0, 0},
    {WENA_MEMBER_CHANGE_SETTINGS, WENA_ICON_GEAR, "change-settings", 1, 0},
    {WENA_MEMBER_CHANGE_COLOR, WENA_ICON_BRUSH, "change-color", 0, 0},
    {WENA_MEMBER_NOTIFICATIONS, WENA_ICON_BELL, "notifications", 0, 0},
    {WENA_MEMBER_CHANGE_FONT, WENA_ICON_FONT, "change-font", 0, 0},
    {WENA_MEMBER_CHANGE_AVATAR, WENA_ICON_PICTURE, "change-avatar", 0, 0},
    {WENA_MEMBER_CHANGE_PASSWORD, WENA_ICON_KEY, "changePasswordPopup-title", 0, 0},
    {WENA_MEMBER_CHANGE_LANGUAGE, WENA_ICON_FLAG, "changeLanguagePopup-title", 1, 0},
    {WENA_MEMBER_LOG_OUT, WENA_ICON_SIGN_OUT, "log-out", 0, 1}};

int wena_member_item_enabled(WenaMemberItem item)
{
    size_t index;
    for (index = 0; index < sizeof(entries) / sizeof(entries[0]); ++index)
        if (entries[index].item == item) return entries[index].enabled;
    return 0;
}

size_t wena_member_menu_items(WenaWekanMenuItem *items, WenaMemberItem *ids, size_t capacity, int is_admin)
{
    size_t index, count = 0;
    if (items == NULL || ids == NULL) return 0;
    for (index = 0; index < sizeof(entries) / sizeof(entries[0]) && count < capacity; ++index) {
        if (entries[index].item == WENA_MEMBER_ADMIN_PANEL && !is_admin) continue;
        items[count].icon = entries[index].icon;
        items[count].text = wena_ui_key_text(entries[index].key, NULL);
        items[count].enabled = entries[index].enabled;
        items[count].separator_before = entries[index].separator_before;
        items[count].checked = 0;
        ids[count++] = entries[index].item;
    }
    return count;
}

static void set_field(char *out, int *length, const char *text)
{
    size_t size = text != NULL ? strlen(text) : 0;
    if (size >= WENA_MEMBER_FIELD) size = WENA_MEMBER_FIELD - 1;
    if (size > 0) memcpy(out, text, size);
    out[size] = '\0';
    *length = (int)size;
}

void wena_member_profile_open(WenaMemberProfileForm *form, const WenaWekanProfile *profile)
{
    memset(form, 0, sizeof(*form));
    if (profile != NULL) {
        set_field(form->fullname, &form->fullname_length, profile->fullname);
        set_field(form->username, &form->username_length, profile->username);
        set_field(form->initials, &form->initials_length, profile->initials);
        set_field(form->email, &form->email_length, profile->email);
    }
    form->visible = 1;
    form->focus = 1;
    form->error = WENA_WEKAN_PROFILE_SAVED;
}

static void read_field(char *out, size_t capacity, const char *text, int length)
{
    size_t size = length < 0 ? 0 : (size_t)length;
    if (size >= capacity) size = capacity - 1;
    memcpy(out, text, size);
    out[size] = '\0';
}

void wena_member_profile_read(const WenaMemberProfileForm *form, WenaWekanProfile *profile)
{
    memset(profile, 0, sizeof(*profile));
    read_field(profile->fullname, sizeof(profile->fullname), form->fullname, form->fullname_length);
    read_field(profile->username, sizeof(profile->username), form->username, form->username_length);
    read_field(profile->initials, sizeof(profile->initials), form->initials, form->initials_length);
    read_field(profile->email, sizeof(profile->email), form->email, form->email_length);
}

/* The popup's frame: white, WeKan's gray title and its close cross. */
static int popup_begin(struct nk_context *context, const char *name, const char *title,
                       float width, float height, float w, float h, int *closed)
{
    nk_style_push_style_item(context, &context->style.window.fixed_background,
                             nk_style_item_color(nk_rgb(255, 255, 255)));
    if (!nk_begin(context, name, nk_rect(width - w - 20.0f > 0.0f ? width - w - 20.0f : 0.0f, 80.0f, w,
                                         h < height - 90.0f ? h : height - 90.0f),
                  NK_WINDOW_BORDER)) return 0;
    nk_layout_row_begin(context, NK_DYNAMIC, 28.0f, 2);
    nk_layout_row_push(context, 0.88f);
    wena_wekan_text(context, title, WENA_WEKAN_FONT_BOLD, WENA_WEKAN_POPUP_HEADER_TEXT, NK_TEXT_CENTERED);
    nk_layout_row_push(context, 0.10f);
    if (wena_wekan_icon_button(context, WENA_ICON_TIMES, wena_ui_key_text("close", "Close"), 14.0f, WENA_WEKAN_ICON))
        *closed = 1;
    nk_layout_row_end(context);
    return 1;
}

static void popup_end(struct nk_context *context)
{
    nk_end(context);
    nk_style_pop_style_item(context);
}

static void labelled_field(struct nk_context *context, const char *label, char *text, int *length, int focus)
{
    nk_layout_row_dynamic(context, 20.0f, 1);
    wena_wekan_text(context, label, WENA_WEKAN_FONT_BOLD, WENA_WEKAN_SECTION_TITLE, NK_TEXT_LEFT);
    nk_layout_row_dynamic(context, 30.0f, 1);
    if (focus) nk_edit_focus(context, NK_EDIT_FIELD);
    (void)nk_edit_string(context, NK_EDIT_FIELD, text, length, WENA_MEMBER_FIELD - 1, nk_filter_default);
}

static const char *profile_error(int error)
{
    switch (error) {
    case WENA_WEKAN_PROFILE_USERNAME_TAKEN: return wena_ui_key_text("error-username-taken", NULL);
    case WENA_WEKAN_PROFILE_EMAIL_TAKEN: return wena_ui_key_text("error-email-taken", NULL);
    case WENA_WEKAN_PROFILE_BAD_USERNAME: return wena_ui_key_text("username", NULL);
    case WENA_WEKAN_PROFILE_BAD_EMAIL: return wena_ui_key_text("email", NULL);
    case WENA_WEKAN_PROFILE_FAILED: return wena_ui_key_text("error-undefined", NULL);
    default: return NULL;
    }
}

int wena_member_profile_render(struct nk_context *context, WenaMemberProfileForm *form, float width, float height)
{
    int result = 0, closed = 0;
    const char *error;
    if (form == NULL || !form->visible) return 0;
    if (popup_begin(context, "Wena edit profile", wena_ui_key_text("editProfilePopup-title", NULL), width, height,
                    340.0f, 400.0f, &closed)) {
        labelled_field(context, wena_ui_key_text("fullname", NULL), form->fullname, &form->fullname_length,
                       form->focus);
        form->focus = 0;
        labelled_field(context, wena_ui_key_text("username", NULL), form->username, &form->username_length, 0);
        labelled_field(context, wena_ui_key_text("initials", NULL), form->initials, &form->initials_length, 0);
        labelled_field(context, wena_ui_key_text("email", NULL), form->email, &form->email_length, 0);
        error = profile_error(form->error);
        nk_layout_row_dynamic(context, 22.0f, 1);
        if (error != NULL) wena_wekan_text(context, error, WENA_WEKAN_FONT_BODY, WENA_WEKAN_WIP_EXCEEDED, NK_TEXT_LEFT);
        else nk_label(context, "", NK_TEXT_LEFT);
        nk_layout_row_dynamic(context, 34.0f, 1);
        if (wena_wekan_button(context, wena_ui_key_text("save", NULL), WENA_WEKAN_BUTTON) && form->username_length > 0)
            result = WENA_MEMBER_SAVE;
    }
    popup_end(context);
    if (closed) { form->visible = 0; return WENA_MEMBER_CLOSED; }
    return result;
}

static const struct {
    WenaMemberToggle toggle;
    WenaIcon icon;
    const char *key;
    const char *field;
} toggles[] = {
    {WENA_MEMBER_TOGGLE_DRAG_HANDLES, WENA_ICON_ARROWS, "show-desktop-drag-handles", "showDesktopDragHandles"},
    {WENA_MEMBER_TOGGLE_SUBMIT_ON_ENTER, WENA_ICON_ARROW_DOWN, "submit-on-enter", "submitOnEnter"},
    {WENA_MEMBER_TOGGLE_OPEN_MANY, WENA_ICON_WINDOW_MAXIMIZE, "open-many-cards-at-once", "openManyCardsAtOnce"},
    {WENA_MEMBER_TOGGLE_CHECKLIST_DING, WENA_ICON_BELL, "checklist-ding-sound", "checklistDingSound"},
    {WENA_MEMBER_TOGGLE_RESCUE_DESCRIPTION, WENA_ICON_NONE, "rescue-card-description", "rescueCardDescription"}};

const char *wena_member_toggle_field(WenaMemberToggle toggle)
{
    size_t index;
    for (index = 0; index < sizeof(toggles) / sizeof(toggles[0]); ++index)
        if (toggles[index].toggle == toggle) return toggles[index].field;
    return NULL;
}

void wena_member_settings_open(WenaMemberSettingsForm *form, const int on[WENA_MEMBER_TOGGLE_COUNT], long count)
{
    int index;
    memset(form, 0, sizeof(*form));
    for (index = 0; index < WENA_MEMBER_TOGGLE_COUNT; ++index) form->on[index] = on != NULL && on[index] != 0;
    sprintf(form->count, "%ld", count < -1 ? -1L : count > 100000L ? 100000L : count);
    form->count_length = (int)strlen(form->count);
    form->visible = 1;
}

int wena_member_settings_count(const WenaMemberSettingsForm *form, long *count)
{
    char text[16], *end;
    long value;
    if (form == NULL || count == NULL || form->count_length <= 0 || form->count_length >= (int)sizeof(text)) return 0;
    memcpy(text, form->count, (size_t)form->count_length);
    text[form->count_length] = '\0';
    value = strtol(text, &end, 10);
    if (*end != '\0' || value < -1 || value > 100000L) return 0;
    *count = value;
    return 1;
}

int wena_member_settings_render(struct nk_context *context, WenaMemberSettingsForm *form, float width, float height)
{
    int result = 0, closed = 0;
    size_t index;
    long count;
    if (form == NULL || !form->visible) return 0;
    if (popup_begin(context, "Wena change settings", wena_ui_key_text("changeSettingsPopup-title", NULL), width,
                    height, 480.0f, 420.0f, &closed)) {
        /* WeKan's first four: a row each, the check after the current choice. */
        for (index = 0; index < 4; ++index) {
            struct nk_rect row;
            nk_layout_row_dynamic(context, 30.0f, 1);
            row = nk_widget_bounds(context);
            if (wena_wekan_link(context, toggles[index].icon, wena_ui_key_text(toggles[index].key, NULL),
                                WENA_WEKAN_FONT_BOLD, WENA_WEKAN_TEXT))
                result = (int)toggles[index].toggle + 2;
            if (form->on[toggles[index].toggle])
                wena_wekan_icon_draw(context, WENA_ICON_CHECK, row.x + row.w - 20.0f, row.y + 8.0f, 13.0f,
                                     WENA_WEKAN_ICON_ACTIVE);
        }
        nk_layout_row_dynamic(context, 24.0f, 1);
        wena_wekan_text(context, wena_ui_key_text("show-cards-minimum-count", NULL), WENA_WEKAN_FONT_BOLD,
                        WENA_WEKAN_TEXT, NK_TEXT_LEFT);
        nk_layout_row_begin(context, NK_STATIC, 30.0f, 1);
        nk_layout_row_push(context, 90.0f);
        (void)nk_edit_string(context, NK_EDIT_FIELD, form->count, &form->count_length, (int)sizeof(form->count) - 1,
                             nk_filter_decimal);
        nk_layout_row_end(context);
        nk_layout_row_dynamic(context, 24.0f, 1);
        wena_wekan_text(context, wena_ui_key_text("card-settings", NULL), WENA_WEKAN_FONT_BOLD, WENA_WEKAN_TEXT,
                        NK_TEXT_LEFT);
        nk_layout_row_dynamic(context, 44.0f, 1);
        if (wena_wekan_checkbox(context, form->on[WENA_MEMBER_TOGGLE_RESCUE_DESCRIPTION],
                                wena_ui_key_text("rescue-card-description", NULL), 1))
            form->on[WENA_MEMBER_TOGGLE_RESCUE_DESCRIPTION] = !form->on[WENA_MEMBER_TOGGLE_RESCUE_DESCRIPTION];
        nk_layout_row_dynamic(context, 34.0f, 1);
        if (wena_wekan_button(context, wena_ui_key_text("save", NULL), WENA_WEKAN_BUTTON) &&
            wena_member_settings_count(form, &count))
            result = WENA_MEMBER_SAVE;
    }
    popup_end(context);
    if (closed) { form->visible = 0; return WENA_MEMBER_CLOSED; }
    return result;
}
