#include "admin_panel.h"
#include "../common/wekan_look.h"
#include "../../../imports/ui/page_contract.h"
#include "../../platform/nuklear_options.h"

#include <nuklear.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define HEADER 50.0f
#define MENU_WIDTH 230.0f

/* The tabs of WeKan's settingHeader, and the panes of each tab's left menu
 * (settingBody.js and peopleBody.js), in WeKan's order. */
static const struct {
    WenaAdminTab tab;
    WenaIcon icon;
    const char *key;
    int enabled;
} tabs[] = {
    {WENA_ADMIN_TAB_SETTINGS, WENA_ICON_GEAR, "settings", 1},
    {WENA_ADMIN_TAB_PEOPLE, WENA_ICON_USERS, "people", 1},
    {WENA_ADMIN_TAB_ATTACHMENTS, WENA_ICON_PAPERCLIP, "attachments", 0},
    {WENA_ADMIN_TAB_PROBLEMS, WENA_ICON_BAN, "problems", 0}};

static const struct {
    WenaAdminPane pane;
    WenaAdminTab tab;
    WenaIcon icon;
    const char *key;
    const char *label;          /* WeKan's own words where it has no key */
    int enabled;
} panes[] = {
    {WENA_ADMIN_PANE_VERSION, WENA_ADMIN_TAB_SETTINGS, WENA_ICON_FILE_TEXT_O, "info", NULL, 1},
    {WENA_ADMIN_PANE_VISIBILITY, WENA_ADMIN_TAB_SETTINGS, WENA_ICON_EYE, "visibility", NULL, 0},
    {WENA_ADMIN_PANE_ANNOUNCEMENT, WENA_ADMIN_TAB_SETTINGS, WENA_ICON_BELL, "admin-announcement", NULL, 1},
    {WENA_ADMIN_PANE_ACCESSIBILITY, WENA_ADMIN_TAB_SETTINGS, WENA_ICON_USER, "accessibility", NULL, 0},
    {WENA_ADMIN_PANE_TRANSLATION, WENA_ADMIN_TAB_SETTINGS, WENA_ICON_GLOBE, "translation", NULL, 0},
    {WENA_ADMIN_PANE_PWA, WENA_ADMIN_TAB_SETTINGS, WENA_ICON_MOBILE, NULL, "PWA", 0},
    {WENA_ADMIN_PANE_WEBHOOKS, WENA_ADMIN_TAB_SETTINGS, WENA_ICON_GLOBE, "global-webhook", NULL, 0},
    {WENA_ADMIN_PANE_EMAIL, WENA_ADMIN_TAB_PEOPLE, WENA_ICON_FILE_TEXT_O, "email", NULL, 0},
    {WENA_ADMIN_PANE_NOTIFICATIONS, WENA_ADMIN_TAB_PEOPLE, WENA_ICON_BELL, "notifications", NULL, 0},
    {WENA_ADMIN_PANE_DOMAINS, WENA_ADMIN_TAB_PEOPLE, WENA_ICON_GLOBE, "domains", NULL, 0},
    {WENA_ADMIN_PANE_ORGANIZATIONS, WENA_ADMIN_TAB_PEOPLE, WENA_ICON_GLOBE, "organizations", NULL, 0},
    {WENA_ADMIN_PANE_TEAMS, WENA_ADMIN_TAB_PEOPLE, WENA_ICON_USERS, "teams", NULL, 0},
    {WENA_ADMIN_PANE_PEOPLE, WENA_ADMIN_TAB_PEOPLE, WENA_ICON_USER, "people", NULL, 1},
    {WENA_ADMIN_PANE_LOCKED_USERS, WENA_ADMIN_TAB_PEOPLE, WENA_ICON_LOCK, "accounts-lockout-locked-users", NULL, 0},
    {WENA_ADMIN_PANE_ROLES, WENA_ADMIN_TAB_PEOPLE, WENA_ICON_KEY, "roles", NULL, 0},
    {WENA_ADMIN_PANE_SHARED_TEMPLATES, WENA_ADMIN_TAB_PEOPLE, WENA_ICON_CLIPBOARD, "shared-templates", NULL, 0},
    {WENA_ADMIN_PANE_LOGIN, WENA_ADMIN_TAB_PEOPLE, WENA_ICON_KEY, "login", NULL, 1},
    {WENA_ADMIN_PANE_SAML, WENA_ADMIN_TAB_PEOPLE, WENA_ICON_KEY, NULL, "SAML", 0},
    {WENA_ADMIN_PANE_LDAP, WENA_ADMIN_TAB_PEOPLE, WENA_ICON_USERS, "ldap", NULL, 0},
    {WENA_ADMIN_PANE_OAUTH, WENA_ADMIN_TAB_PEOPLE, WENA_ICON_KEY, "oauth-providers-title", NULL, 0},
    {WENA_ADMIN_PANE_PASSWORDLESS, WENA_ADMIN_TAB_PEOPLE, WENA_ICON_LOCK, "passwordless-title", NULL, 0}};

#define PANE_COUNT (sizeof(panes) / sizeof(panes[0]))

WenaAdminTab wena_admin_pane_tab(WenaAdminPane pane)
{
    size_t index;
    for (index = 0; index < PANE_COUNT; ++index) if (panes[index].pane == pane) return panes[index].tab;
    return WENA_ADMIN_TAB_COUNT;
}

const char *wena_admin_pane_label(WenaAdminPane pane)
{
    size_t index;
    for (index = 0; index < PANE_COUNT; ++index)
        if (panes[index].pane == pane)
            return panes[index].key != NULL ? wena_ui_key_text(panes[index].key, NULL) : panes[index].label;
    return "";
}

int wena_admin_pane_enabled(WenaAdminPane pane)
{
    size_t index;
    for (index = 0; index < PANE_COUNT; ++index) if (panes[index].pane == pane) return panes[index].enabled;
    return 0;
}

WenaAdminPane wena_admin_tab_first(WenaAdminTab tab)
{
    return tab == WENA_ADMIN_TAB_PEOPLE ? WENA_ADMIN_PANE_PEOPLE : WENA_ADMIN_PANE_VERSION;
}

void wena_admin_panel_open(WenaAdminPanel *panel, WenaAdminTab tab)
{
    if (panel == NULL) return;
    panel->open = 1;
    panel->tab = tab == WENA_ADMIN_TAB_PEOPLE ? WENA_ADMIN_TAB_PEOPLE : WENA_ADMIN_TAB_SETTINGS;
    panel->pane = wena_admin_tab_first(panel->tab);
    panel->edit_visible = 0;
    panel->people_error = 0;
}

void wena_admin_panel_edit(WenaAdminPanel *panel, size_t person)
{
    WenaWekanProfile profile;
    const WenaWekanPerson *row;
    if (panel == NULL || panel->people == NULL || person >= panel->people_count) return;
    row = &panel->people[person];
    memset(&profile, 0, sizeof(profile));
    strcpy(profile.fullname, row->fullname);
    strcpy(profile.username, row->username);
    strcpy(profile.email, row->email);
    wena_member_profile_open(&panel->edit, &profile);
    panel->edit.visible = 0;            /* the panel's own popup, not Edit Profile's */
    strcpy(panel->edit_id, row->id);
    panel->edit_admin = row->is_admin;
    panel->edit_active = !row->login_disabled;
    panel->edit_visible = 1;
    panel->people_error = 0;
}

void wena_admin_format_date(double ms, char *out, size_t capacity)
{
    static const char *const months[] = {"January", "February", "March", "April", "May", "June", "July",
                                         "August", "September", "October", "November", "December"};
    time_t seconds;
    struct tm *local;
    char text[64];
    if (out == NULL || capacity == 0) return;
    out[0] = '\0';
    if (ms <= 0.0) return;
    seconds = (time_t)(ms / 1000.0);
    local = localtime(&seconds);
    if (local == NULL || local->tm_mon < 0 || local->tm_mon > 11) return;
    sprintf(text, "%s %d, %d %d:%02d %s", months[local->tm_mon], local->tm_mday, local->tm_year + 1900,
            local->tm_hour % 12 == 0 ? 12 : local->tm_hour % 12, local->tm_min, local->tm_hour < 12 ? "AM" : "PM");
    if (strlen(text) < capacity) strcpy(out, text);
}

static void put(struct nk_context *context, float x, float y, float w, float h)
{
    nk_layout_space_push(context, nk_rect(x, y, w, h));
}

static void text_at(struct nk_context *context, const char *text, WenaWekanFont font, int color,
                    float x, float y, float w, float h)
{
    put(context, x, y, w, h);
    wena_wekan_text(context, text, font, color, NK_TEXT_LEFT);
}

/* A table cell; a disabled account's is struck through, as WeKan's <s>. */
static void cell(struct nk_context *context, const char *text, int struck, float w)
{
    struct nk_rect bounds;
    nk_layout_row_push(context, w);
    bounds = nk_widget_bounds(context);
    wena_wekan_text(context, text, WENA_WEKAN_FONT_BODY, struck ? WENA_WEKAN_ICON : WENA_WEKAN_TEXT, NK_TEXT_LEFT);
    if (struck && text[0] != '\0') {
        const struct nk_user_font *face = wena_wekan_font(context, WENA_WEKAN_FONT_BODY);
        float length = face != NULL ? face->width(face->userdata, face->height, text, (int)strlen(text)) : bounds.w;
        nk_stroke_line(nk_window_get_canvas(context), bounds.x, bounds.y + bounds.h / 2.0f,
                       bounds.x + (length < bounds.w ? length : bounds.w), bounds.y + bounds.h / 2.0f, 1.0f,
                       nk_rgb(0x4d, 0x4d, 0x4d));
    }
}

static unsigned int people_pane(struct nk_context *context, WenaAdminPanel *panel, size_t *person)
{
    /* Shares of the pane's width: Edit, Username, Email, Admin, Active, Created. */
    static const float widths[] = {0.09f, 0.18f, 0.25f, 0.09f, 0.14f, 0.25f};
    static const char *const heads[] = {"", "username", "email", "admin", "active-person", "createdAt"};
    unsigned int action = WENA_ADMIN_NONE;
    size_t index, column;
    char created[64];
    nk_layout_row_begin(context, NK_DYNAMIC, 26.0f, 6);
    for (column = 0; column < 6; ++column) {
        nk_layout_row_push(context, widths[column]);
        wena_wekan_text(context, heads[column][0] ? wena_ui_key_text(heads[column], NULL) : "", WENA_WEKAN_FONT_BOLD,
                        WENA_WEKAN_TEXT, NK_TEXT_LEFT);
    }
    nk_layout_row_end(context);
    for (index = 0; panel->people != NULL && index < panel->people_count; ++index) {
        const WenaWekanPerson *row = &panel->people[index];
        struct nk_rect active;
        nk_layout_row_begin(context, NK_DYNAMIC, 30.0f, 6);
        nk_layout_row_push(context, widths[0]);
        if (wena_wekan_link(context, WENA_ICON_PENCIL, wena_ui_key_text("edit", "Edit"), WENA_WEKAN_FONT_SMALL,
                            WENA_WEKAN_BUTTON))
            wena_admin_panel_edit(panel, index);
        cell(context, row->username, row->login_disabled, widths[1]);
        cell(context, row->email, row->login_disabled, widths[2]);
        cell(context, wena_ui_key_text(row->is_admin ? "yes" : "no", row->is_admin ? "Yes" : "No"),
             row->login_disabled, widths[3]);
        /* WeKan's Active: a green check or a red ban, clicked to change. */
        nk_layout_row_push(context, widths[4]);
        active = nk_widget_bounds(context);
        if (wena_wekan_icon_button(context, row->login_disabled ? WENA_ICON_BAN : WENA_ICON_CHECK,
                                   wena_ui_key_text(row->login_disabled ? "admin-people-user-inactive"
                                                                        : "admin-people-user-active", NULL),
                                   14.0f, row->login_disabled ? WENA_WEKAN_WIP_EXCEEDED : WENA_WEKAN_BUTTON)) {
            action |= WENA_ADMIN_TOGGLE_ACTIVE;
            if (person != NULL) *person = index;
        }
        (void)active;
        wena_admin_format_date(row->created_at, created, sizeof(created));
        cell(context, created, row->login_disabled, widths[5]);
        nk_layout_row_end(context);
    }
    if (panel->people_error == WENA_WEKAN_PERSON_LAST_ADMIN) {
        nk_layout_row_dynamic(context, 26.0f, 1);
        wena_wekan_text(context, wena_ui_key_text("last-admin-desc", NULL), WENA_WEKAN_FONT_BOLD,
                        WENA_WEKAN_WIP_EXCEEDED, NK_TEXT_LEFT);
    }
    return action;
}

static void yes_no(struct nk_context *context, const char *label, int *value)
{
    nk_layout_row_dynamic(context, 20.0f, 1);
    wena_wekan_text(context, label, WENA_WEKAN_FONT_BOLD, WENA_WEKAN_SECTION_TITLE, NK_TEXT_LEFT);
    nk_layout_row_begin(context, NK_STATIC, 30.0f, 2);
    nk_layout_row_push(context, 90.0f);
    if (wena_wekan_checkbox(context, *value, wena_ui_key_text("yes", "Yes"), 1)) *value = 1;
    nk_layout_row_push(context, 90.0f);
    if (wena_wekan_checkbox(context, !*value, wena_ui_key_text("no", "No"), 1)) *value = 0;
    nk_layout_row_end(context);
}

static void edit_field(struct nk_context *context, const char *label, char *text, int *length)
{
    nk_layout_row_dynamic(context, 20.0f, 1);
    wena_wekan_text(context, label, WENA_WEKAN_FONT_BOLD, WENA_WEKAN_SECTION_TITLE, NK_TEXT_LEFT);
    nk_layout_row_dynamic(context, 30.0f, 1);
    (void)nk_edit_string(context, NK_EDIT_FIELD, text, length, WENA_MEMBER_FIELD - 1, nk_filter_default);
}

/* WeKan's editUserPopup: Username, Full Name, Initials, Admin, Email, Active. */
static unsigned int edit_user(struct nk_context *context, WenaAdminPanel *panel, float width, float height)
{
    unsigned int action = WENA_ADMIN_NONE;
    int closed = 0;
    float w = 360.0f, h = 520.0f < height - 70.0f ? 520.0f : height - 70.0f;
    nk_style_push_style_item(context, &context->style.window.fixed_background,
                             nk_style_item_color(nk_rgb(255, 255, 255)));
    if (nk_begin(context, "Wena edit user", nk_rect((width - w) / 2.0f, 60.0f, w, h), NK_WINDOW_BORDER)) {
        const char *error = NULL;
        nk_layout_row_begin(context, NK_DYNAMIC, 28.0f, 2);
        nk_layout_row_push(context, 0.88f);
        wena_wekan_text(context, wena_ui_key_text("editUserPopup-title", NULL), WENA_WEKAN_FONT_BOLD,
                        WENA_WEKAN_POPUP_HEADER_TEXT, NK_TEXT_CENTERED);
        nk_layout_row_push(context, 0.10f);
        if (wena_wekan_icon_button(context, WENA_ICON_TIMES, wena_ui_key_text("close", "Close"), 14.0f, WENA_WEKAN_ICON))
            closed = 1;
        nk_layout_row_end(context);
        edit_field(context, wena_ui_key_text("username", NULL), panel->edit.username, &panel->edit.username_length);
        edit_field(context, wena_ui_key_text("fullname", NULL), panel->edit.fullname, &panel->edit.fullname_length);
        edit_field(context, wena_ui_key_text("initials", NULL), panel->edit.initials, &panel->edit.initials_length);
        yes_no(context, wena_ui_key_text("admin", NULL), &panel->edit_admin);
        edit_field(context, wena_ui_key_text("email", NULL), panel->edit.email, &panel->edit.email_length);
        yes_no(context, wena_ui_key_text("active-person", NULL), &panel->edit_active);
        switch (panel->edit.error) {
        case WENA_WEKAN_PROFILE_USERNAME_TAKEN: error = wena_ui_key_text("error-username-taken", NULL); break;
        case WENA_WEKAN_PROFILE_EMAIL_TAKEN: error = wena_ui_key_text("error-email-taken", NULL); break;
        case WENA_WEKAN_PROFILE_BAD_USERNAME: error = wena_ui_key_text("username", NULL); break;
        case WENA_WEKAN_PROFILE_BAD_EMAIL: error = wena_ui_key_text("email", NULL); break;
        default: break;
        }
        if (panel->people_error == WENA_WEKAN_PERSON_LAST_ADMIN) error = wena_ui_key_text("last-admin-desc", NULL);
        nk_layout_row_dynamic(context, 22.0f, 1);
        if (error != NULL) wena_wekan_text(context, error, WENA_WEKAN_FONT_BODY, WENA_WEKAN_WIP_EXCEEDED, NK_TEXT_LEFT);
        else nk_label(context, "", NK_TEXT_LEFT);
        nk_layout_row_dynamic(context, 34.0f, 1);
        if (wena_wekan_button(context, wena_ui_key_text("save", NULL), WENA_WEKAN_BUTTON) &&
            panel->edit.username_length > 0)
            action |= WENA_ADMIN_EDIT_SAVE;
    }
    nk_end(context);
    nk_style_pop_style_item(context);
    if (closed) panel->edit_visible = 0;
    return action;
}

static unsigned int pane_body(struct nk_context *context, WenaAdminPanel *panel, size_t *person)
{
    unsigned int action = WENA_ADMIN_NONE;
    size_t line;
    switch (panel->pane) {
    case WENA_ADMIN_PANE_PEOPLE:
        action |= people_pane(context, panel, person);
        break;
    case WENA_ADMIN_PANE_VERSION:
        for (line = 0; line < panel->info_count && line < WENA_ADMIN_INFO_LINES; ++line) {
            nk_layout_row_begin(context, NK_STATIC, 26.0f, 2);
            nk_layout_row_push(context, 180.0f);
            wena_wekan_text(context, panel->info_label[line] != NULL ? panel->info_label[line] : "", WENA_WEKAN_FONT_BOLD,
                            WENA_WEKAN_TEXT, NK_TEXT_LEFT);
            nk_layout_row_push(context, 420.0f);
            wena_wekan_text(context, panel->info_value[line], WENA_WEKAN_FONT_BODY, WENA_WEKAN_TEXT, NK_TEXT_LEFT);
            nk_layout_row_end(context);
        }
        break;
    case WENA_ADMIN_PANE_ANNOUNCEMENT:
        nk_layout_row_dynamic(context, 30.0f, 1);
        if (wena_wekan_checkbox(context, panel->announcement_enabled,
                                wena_ui_key_text("admin-announcement-active", NULL), 1))
            panel->announcement_enabled = !panel->announcement_enabled;
        /* WeKan hides the message while the announcement is off. */
        if (panel->announcement_enabled) {
            nk_layout_row_dynamic(context, 22.0f, 1);
            wena_wekan_text(context, wena_ui_key_text("admin-announcement-title", NULL), WENA_WEKAN_FONT_BOLD,
                            WENA_WEKAN_SECTION_TITLE, NK_TEXT_LEFT);
            nk_layout_row_dynamic(context, 120.0f, 1);
            (void)nk_edit_string(context, NK_EDIT_BOX, panel->announcement, &panel->announcement_length,
                                 (int)sizeof(panel->announcement) - 1, nk_filter_default);
        }
        nk_layout_row_begin(context, NK_STATIC, 34.0f, 1);
        nk_layout_row_push(context, 120.0f);
        if (wena_wekan_button(context, wena_ui_key_text("save", NULL), WENA_WEKAN_BUTTON))
            action |= WENA_ADMIN_ANNOUNCEMENT_SAVE;
        nk_layout_row_end(context);
        break;
    case WENA_ADMIN_PANE_LOGIN:
        nk_layout_row_dynamic(context, 28.0f, 1);
        wena_wekan_text(context, wena_ui_key_text("login-allow", NULL), WENA_WEKAN_FONT_SECTION, WENA_WEKAN_TEXT,
                        NK_TEXT_LEFT);
        /* Ticked when allowed; WeKan stores the "disable" flags. Until WeKan
         * has run on this file there are none to change. */
        if (!panel->login_available) {
            nk_layout_row_dynamic(context, 44.0f, 1);
            wena_wekan_text_wrap(context, wena_ui_key_text("wena-login-needs-wekan",
                                 "WeKan has not opened these files yet: its login settings are made when it does."),
                                 WENA_WEKAN_FONT_BODY, WENA_WEKAN_ICON);
        }
        nk_layout_row_dynamic(context, 30.0f, 1);
        if (wena_wekan_checkbox(context, !panel->disable_forgot_password, wena_ui_key_text("forgot-password", NULL),
                                panel->login_available)) {
            panel->disable_forgot_password = !panel->disable_forgot_password;
            action |= WENA_ADMIN_LOGIN;
        }
        nk_layout_row_dynamic(context, 30.0f, 1);
        if (wena_wekan_checkbox(context, !panel->disable_registration, wena_ui_key_text("self-registration", NULL),
                                panel->login_available)) {
            panel->disable_registration = !panel->disable_registration;
            action |= WENA_ADMIN_LOGIN;
        }
        break;
    default:
        break;
    }
    return action;
}

unsigned int wena_admin_panel_render(struct nk_context *context, WenaAdminPanel *panel, float width, float height,
                                     size_t *person)
{
    unsigned int action = WENA_ADMIN_NONE;
    struct nk_rect area;
    size_t index;
    float x, y;
    if (context == NULL || panel == NULL || !panel->open) return action;
    if ((int)panel->pane < 0 || panel->pane >= WENA_ADMIN_PANE_COUNT || !wena_admin_pane_enabled(panel->pane))
        panel->pane = wena_admin_tab_first(panel->tab);
    nk_style_push_style_item(context, &context->style.window.fixed_background,
                             nk_style_item_color(nk_rgb((wena_wekan_rgb(WENA_WEKAN_BODY) >> 16) & 255,
                                                        (wena_wekan_rgb(WENA_WEKAN_BODY) >> 8) & 255,
                                                        wena_wekan_rgb(WENA_WEKAN_BODY) & 255)));
    nk_style_push_vec2(context, &context->style.window.padding, nk_vec2(0.0f, 0.0f));
    nk_style_push_vec2(context, &context->style.window.spacing, nk_vec2(0.0f, 0.0f));
    if (nk_begin(context, "Admin Panel", nk_rect(0.0f, 0.0f, width, height), NK_WINDOW_NO_SCROLLBAR)) {
        wena_ui_region("admin-panel");
        nk_layout_space_begin(context, NK_STATIC, height, 64);
        wena_wekan_space_area(context, height, &area.x, &area.y, &area.w);
        area.h = height;
        /* settingHeader: the house back to the board, the name, the tabs. */
        wena_ui_region("header");
        wena_wekan_fill(context, area.x, area.y, area.w, HEADER, WENA_WEKAN_HEADER, 0.0f);
        put(context, 16.0f, 13.0f, 32.0f, 24.0f);
        if (wena_wekan_icon_button(context, WENA_ICON_HOME, wena_ui_key_text("all-boards", NULL), 18.0f,
                                   WENA_WEKAN_HEADER_TEXT))
            action |= WENA_ADMIN_CLOSE;
        text_at(context, wena_ui_key_text("admin-panel", NULL), WENA_WEKAN_FONT_BOLD, WENA_WEKAN_HEADER_TEXT,
                60.0f, 11.0f, 150.0f, 28.0f);
        x = 220.0f;
        for (index = 0; index < sizeof(tabs) / sizeof(tabs[0]); ++index) {
            const char *label = wena_ui_key_text(tabs[index].key, NULL);
            put(context, x, 11.0f, 130.0f, 28.0f);
            if (tabs[index].enabled) {
                if (wena_wekan_link(context, tabs[index].icon, label, WENA_WEKAN_FONT_LINK,
                                    panel->tab == tabs[index].tab ? WENA_WEKAN_HEADER_TEXT : WENA_WEKAN_HEADER_LINK) &&
                    panel->tab != tabs[index].tab) {
                    panel->tab = tabs[index].tab;
                    panel->pane = wena_admin_tab_first(panel->tab);
                    panel->edit_visible = 0;
                    action |= WENA_ADMIN_PANE;
                }
                if (panel->tab == tabs[index].tab)
                    wena_wekan_fill(context, area.x + x, area.y + HEADER - 4.0f, 110.0f, 3.0f, WENA_WEKAN_HEADER_TEXT, 0.0f);
            } else wena_wekan_text(context, label, WENA_WEKAN_FONT_LINK, WENA_WEKAN_ICON, NK_TEXT_LEFT);
            x += 140.0f;
        }
        /* The left menu: this tab's panes; what Wena has not, disabled. */
        wena_ui_region("left-menu");
        wena_wekan_fill(context, area.x, area.y + HEADER, MENU_WIDTH, height - HEADER, WENA_WEKAN_PANEL, 0.0f);
        y = HEADER + 12.0f;
        for (index = 0; index < PANE_COUNT; ++index) {
            const char *label;
            if (panes[index].tab != panel->tab) continue;
            label = panes[index].key != NULL ? wena_ui_key_text(panes[index].key, NULL) : panes[index].label;
            if (panes[index].pane == panel->pane)
                wena_wekan_fill(context, area.x + 6.0f, area.y + y - 2.0f, MENU_WIDTH - 12.0f, 30.0f,
                                WENA_WEKAN_MINICARD_SHADOW, 4.0f);
            put(context, 16.0f, y, MENU_WIDTH - 24.0f, 26.0f);
            if (panes[index].enabled) {
                if (wena_wekan_link(context, panes[index].icon, label, WENA_WEKAN_FONT_BOLD, WENA_WEKAN_TEXT) &&
                    panel->pane != panes[index].pane) {
                    panel->pane = panes[index].pane;
                    panel->edit_visible = 0;
                    action |= WENA_ADMIN_PANE;
                }
            } else {
                wena_wekan_icon_draw(context, panes[index].icon, area.x + 16.0f, area.y + y + 6.0f, 13.0f,
                                     WENA_WEKAN_ICON);
                text_at(context, label, WENA_WEKAN_FONT_BOLD, WENA_WEKAN_ICON, 38.0f, y, MENU_WIDTH - 46.0f, 26.0f);
            }
            y += 34.0f;
        }
        /* The pane: its name, then its rows, scrolling. */
        wena_ui_region("admin-pane");
        text_at(context, wena_admin_pane_label(panel->pane), WENA_WEKAN_FONT_SECTION, WENA_WEKAN_TEXT,
                MENU_WIDTH + 24.0f, HEADER + 14.0f, area.w - MENU_WIDTH - 48.0f, 26.0f);
        put(context, MENU_WIDTH + 24.0f, HEADER + 50.0f, area.w - MENU_WIDTH - 40.0f, height - HEADER - 60.0f);
        nk_style_push_vec2(context, &context->style.window.spacing, nk_vec2(4.0f, 6.0f));
        if (nk_group_begin(context, "Admin pane", 0)) {
            action |= pane_body(context, panel, person);
            nk_group_end(context);
        }
        nk_style_pop_vec2(context);
        nk_layout_space_end(context);
    }
    nk_end(context);
    nk_style_pop_vec2(context);
    nk_style_pop_vec2(context);
    nk_style_pop_style_item(context);
    if (panel->edit_visible) action |= edit_user(context, panel, width, height);
    return action;
}
