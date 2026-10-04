/* WeKan's Admin Panel, drawn with the fake Nuklear: its tabs and left menus
 * in WeKan's order, what Wena has enabled, People's Edit and Active, Edit
 * User's Save, Announcement's Save, Login's checkboxes, and the date. */
#include "../client/components/settings/admin_panel.h"
#include "../client/platform/nuklear_options.h"
#include <nuklear.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    WenaAdminPanel panel;
    WenaWekanPerson people[2];
    struct nk_context context;
    size_t person = 99;
    char date[64];
    unsigned int action;

    /* The panes: WeKan's tabs, labels, and only Wena's four enabled. */
    assert(wena_admin_pane_tab(WENA_ADMIN_PANE_PEOPLE) == WENA_ADMIN_TAB_PEOPLE);
    assert(wena_admin_pane_tab(WENA_ADMIN_PANE_ANNOUNCEMENT) == WENA_ADMIN_TAB_SETTINGS);
    assert(!strcmp(wena_admin_pane_label(WENA_ADMIN_PANE_PEOPLE), "People"));
    assert(!strcmp(wena_admin_pane_label(WENA_ADMIN_PANE_VERSION), "Version"));
    assert(!strcmp(wena_admin_pane_label(WENA_ADMIN_PANE_PWA), "PWA"));
    assert(wena_admin_pane_enabled(WENA_ADMIN_PANE_PEOPLE) && wena_admin_pane_enabled(WENA_ADMIN_PANE_LOGIN) &&
           wena_admin_pane_enabled(WENA_ADMIN_PANE_VERSION) && wena_admin_pane_enabled(WENA_ADMIN_PANE_ANNOUNCEMENT));
    assert(!wena_admin_pane_enabled(WENA_ADMIN_PANE_LDAP) && !wena_admin_pane_enabled(WENA_ADMIN_PANE_TEAMS));
    assert(wena_admin_pane_tab(WENA_ADMIN_PANE_COUNT) == WENA_ADMIN_TAB_COUNT);
    assert(wena_admin_tab_first(WENA_ADMIN_TAB_PEOPLE) == WENA_ADMIN_PANE_PEOPLE &&
           wena_admin_tab_first(WENA_ADMIN_TAB_SETTINGS) == WENA_ADMIN_PANE_VERSION);
    /* WeKan's 'LLL' date; none for no date. */
    wena_admin_format_date(0.0, date, sizeof(date));
    assert(date[0] == '\0');
    wena_admin_format_date(1791130000000.0, date, sizeof(date));
    assert(strstr(date, "2026") != NULL && (strstr(date, " AM") != NULL || strstr(date, " PM") != NULL));

    memset(&panel, 0, sizeof(panel));
    memset(people, 0, sizeof(people));
    strcpy(people[0].id, "u1"); strcpy(people[0].username, "ada"); people[0].is_admin = 1;
    strcpy(people[1].id, "u2"); strcpy(people[1].username, "bob"); strcpy(people[1].email, "bob@x.example");
    people[1].login_disabled = 1;
    panel.people = people;
    panel.people_count = 2;
    wena_admin_panel_open(&panel, WENA_ADMIN_TAB_PEOPLE);
    assert(panel.open && panel.pane == WENA_ADMIN_PANE_PEOPLE);
    /* The house closes it; Active toggles its row. */
    memset(&context, 0, sizeof(context));
    context.button_to_press = "All Boards";
    assert(wena_admin_panel_render(&context, &panel, 1024.0f, 720.0f, &person) & WENA_ADMIN_CLOSE);
    context.button_to_press = "User is inactive - click to activate";
    action = wena_admin_panel_render(&context, &panel, 1024.0f, 720.0f, &person);
    assert((action & WENA_ADMIN_TOGGLE_ACTIVE) != 0u && person == 1);
    /* Edit fills Edit User from the row; Save reports it. */
    wena_admin_panel_edit(&panel, 1);
    assert(panel.edit_visible && !strcmp(panel.edit_id, "u2") && !panel.edit_active && !panel.edit_admin);
    assert(!strcmp(panel.edit.email, "bob@x.example") && panel.edit.username_length == 3);
    context.button_to_press = "Save";
    assert(wena_admin_panel_render(&context, &panel, 1024.0f, 720.0f, &person) & WENA_ADMIN_EDIT_SAVE);
    context.button_to_press = "Close";
    (void)wena_admin_panel_render(&context, &panel, 1024.0f, 720.0f, &person);
    assert(!panel.edit_visible);
    /* Negative: an edit of a row that is not there changes nothing. */
    wena_admin_panel_edit(&panel, 5);
    assert(!panel.edit_visible);
    /* Announcement: Save. */
    panel.pane = WENA_ADMIN_PANE_ANNOUNCEMENT;
    panel.tab = WENA_ADMIN_TAB_SETTINGS;
    context.button_to_press = "Save";
    assert(wena_admin_panel_render(&context, &panel, 1024.0f, 720.0f, &person) & WENA_ADMIN_ANNOUNCEMENT_SAVE);
    /* Login: a checkbox writes the "disable" flag the other way; without
     * WeKan's settings nothing can be clicked. */
    panel.pane = WENA_ADMIN_PANE_LOGIN;
    panel.tab = WENA_ADMIN_TAB_PEOPLE;
    panel.login_available = 1;
    context.button_to_press = "Self-Registration";
    action = wena_admin_panel_render(&context, &panel, 1024.0f, 720.0f, &person);
    assert((action & WENA_ADMIN_LOGIN) != 0u && panel.disable_registration == 1);
    panel.login_available = 0;
    panel.disable_registration = 0;
    context.button_to_press = "Self-Registration";
    assert(!(wena_admin_panel_render(&context, &panel, 1024.0f, 720.0f, &person) & WENA_ADMIN_LOGIN));
    assert(!panel.disable_registration);
    /* A disabled pane falls back to the tab's first. */
    panel.pane = WENA_ADMIN_PANE_LDAP;
    (void)wena_admin_panel_render(&context, &panel, 1024.0f, 720.0f, &person);
    assert(panel.pane == WENA_ADMIN_PANE_PEOPLE);
    /* Closed draws nothing. */
    panel.open = 0;
    assert(wena_admin_panel_render(&context, &panel, 1024.0f, 720.0f, &person) == WENA_ADMIN_NONE);
    puts("admin panel tests passed");
    return 0;
}
