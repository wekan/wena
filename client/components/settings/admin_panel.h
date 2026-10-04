#ifndef WENA_ADMIN_PANEL_H
#define WENA_ADMIN_PANEL_H
/* WeKan's Admin Panel (client/components/settings): its header's tabs -
 * Settings, People, Attachments, Problems - each with its left menu
 * (models/lib/leftMenu.js's entries, in WeKan's order), and the panes Wena
 * has: Settings / Version, Settings / Announcement, People / People with
 * Edit User, and People / Login. The other entries are there, disabled.
 * What it shows and saves is WeKan's file, through server/wekan_sync.h; this
 * only draws and edits, and reports what was asked. */
#include <stddef.h>
#include "../users/member_settings.h"
#include "../../../server/wekan_sync.h"

struct nk_context;

typedef enum WenaAdminTab {
    WENA_ADMIN_TAB_SETTINGS,
    WENA_ADMIN_TAB_PEOPLE,
    WENA_ADMIN_TAB_ATTACHMENTS,
    WENA_ADMIN_TAB_PROBLEMS,
    WENA_ADMIN_TAB_COUNT
} WenaAdminTab;

typedef enum WenaAdminPane {
    /* Settings (settingBody.js) */
    WENA_ADMIN_PANE_VERSION, WENA_ADMIN_PANE_VISIBILITY, WENA_ADMIN_PANE_ANNOUNCEMENT,
    WENA_ADMIN_PANE_ACCESSIBILITY, WENA_ADMIN_PANE_TRANSLATION, WENA_ADMIN_PANE_PWA, WENA_ADMIN_PANE_WEBHOOKS,
    /* People (peopleBody.js) */
    WENA_ADMIN_PANE_EMAIL, WENA_ADMIN_PANE_NOTIFICATIONS, WENA_ADMIN_PANE_DOMAINS, WENA_ADMIN_PANE_ORGANIZATIONS,
    WENA_ADMIN_PANE_TEAMS, WENA_ADMIN_PANE_PEOPLE, WENA_ADMIN_PANE_LOCKED_USERS, WENA_ADMIN_PANE_ROLES,
    WENA_ADMIN_PANE_SHARED_TEMPLATES, WENA_ADMIN_PANE_LOGIN, WENA_ADMIN_PANE_SAML, WENA_ADMIN_PANE_LDAP,
    WENA_ADMIN_PANE_OAUTH, WENA_ADMIN_PANE_PASSWORDLESS,
    WENA_ADMIN_PANE_COUNT
} WenaAdminPane;

/* A pane's tab, label and whether Wena has it. */
WenaAdminTab wena_admin_pane_tab(WenaAdminPane pane);
const char *wena_admin_pane_label(WenaAdminPane pane);
int wena_admin_pane_enabled(WenaAdminPane pane);
/* The pane a tab opens on: People's People, Settings' Version. */
WenaAdminPane wena_admin_tab_first(WenaAdminTab tab);

#define WENA_ADMIN_INFO_LINES 8
typedef struct WenaAdminPanel {
    int open;
    WenaAdminTab tab;
    WenaAdminPane pane;
    /* People: given by the caller after a load. */
    const WenaWekanPerson *people;
    size_t people_count;
    /* Edit User (editUserPopup): the person, their fields and Admin/Active. */
    int edit_visible;
    char edit_id[64];
    WenaMemberProfileForm edit;
    int edit_admin, edit_active;
    int people_error;               /* WENA_WEKAN_PERSON_LAST_ADMIN shows WeKan's warning */
    /* Announcement: Active and the message. */
    int announcement_enabled;
    char announcement[2048];
    int announcement_length;
    /* Login: 0 when WeKan has not made its settings yet; the "disable" flags. */
    int login_available, disable_registration, disable_forgot_password;
    /* Version: label and value lines. */
    const char *info_label[WENA_ADMIN_INFO_LINES];
    char info_value[WENA_ADMIN_INFO_LINES][128];
    size_t info_count;
} WenaAdminPanel;

/* What render reports; `person` is the People row for the person ones. */
#define WENA_ADMIN_NONE 0u
#define WENA_ADMIN_CLOSE 1u              /* the house: back to the board */
#define WENA_ADMIN_PANE 2u               /* another pane: reload its data */
#define WENA_ADMIN_TOGGLE_ACTIVE 4u      /* People's Active icon */
#define WENA_ADMIN_EDIT_SAVE 8u          /* Edit User's Save */
#define WENA_ADMIN_ANNOUNCEMENT_SAVE 16u
#define WENA_ADMIN_LOGIN 32u             /* a Login checkbox: written at once */

void wena_admin_panel_open(WenaAdminPanel *panel, WenaAdminTab tab);
/* Fills Edit User from a People row. */
void wena_admin_panel_edit(WenaAdminPanel *panel, size_t person);
/* "October 4, 2026 3:22 PM" from ms, in local time, as WeKan's 'LLL'. */
void wena_admin_format_date(double ms, char *out, size_t capacity);
unsigned int wena_admin_panel_render(struct nk_context *context, WenaAdminPanel *panel, float width, float height,
                                     size_t *person);

#endif
