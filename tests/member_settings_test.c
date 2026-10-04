/* WeKan's member menu, Edit Profile and Change Settings, drawn with the
 * fake Nuklear: the menu's order and who sees Admin Panel, the forms' fields
 * in and out, their buttons, and the card count's bounds. */
#include "../client/components/users/member_settings.h"
#include "../imports/ui/page_contract.h"

#include <assert.h>
#include "../client/platform/nuklear_options.h"
#include <nuklear.h>
#include <stdio.h>
#include <string.h>

static size_t find(const WenaMemberItem *ids, size_t count, WenaMemberItem item)
{
    size_t index;
    for (index = 0; index < count; ++index) if (ids[index] == item) return index;
    return count;
}

int main(void)
{
    WenaWekanMenuItem items[WENA_MEMBER_ITEM_COUNT];
    WenaMemberItem ids[WENA_MEMBER_ITEM_COUNT];
    WenaMemberProfileForm profile;
    WenaMemberSettingsForm settings;
    WenaWekanProfile in, out;
    struct nk_context context;
    int on[WENA_MEMBER_TOGGLE_COUNT] = {1, 0, 1, 0, 1};
    long count;
    size_t admin_count, member_count;

    /* The menu: WeKan's order, Admin Panel only for an admin. */
    admin_count = wena_member_menu_items(items, ids, WENA_MEMBER_ITEM_COUNT, 1);
    assert(admin_count == WENA_MEMBER_ITEM_COUNT && ids[0] == WENA_MEMBER_MY_CARDS &&
           ids[admin_count - 1] == WENA_MEMBER_LOG_OUT);
    assert(find(ids, admin_count, WENA_MEMBER_ADMIN_PANEL) < find(ids, admin_count, WENA_MEMBER_EDIT_PROFILE));
    assert(find(ids, admin_count, WENA_MEMBER_EDIT_PROFILE) < find(ids, admin_count, WENA_MEMBER_CHANGE_SETTINGS));
    assert(!strcmp(items[find(ids, admin_count, WENA_MEMBER_EDIT_PROFILE)].text, "Edit Profile"));
    assert(!strcmp(items[find(ids, admin_count, WENA_MEMBER_CHANGE_LANGUAGE)].text, "Change Language"));
    /* What Wena does is enabled; the rest is there, disabled. */
    assert(items[find(ids, admin_count, WENA_MEMBER_EDIT_PROFILE)].enabled &&
           items[find(ids, admin_count, WENA_MEMBER_CHANGE_SETTINGS)].enabled &&
           items[find(ids, admin_count, WENA_MEMBER_ALL_BOARDS)].enabled);
    assert(!items[find(ids, admin_count, WENA_MEMBER_LOG_OUT)].enabled && !wena_member_item_enabled(WENA_MEMBER_DATE));
    /* Negative: no Admin Panel for a member; a small array is not overrun. */
    member_count = wena_member_menu_items(items, ids, WENA_MEMBER_ITEM_COUNT, 0);
    assert(member_count == admin_count - 1 && find(ids, member_count, WENA_MEMBER_ADMIN_PANEL) == member_count);
    assert(wena_member_menu_items(items, ids, 3, 1) == 3 && wena_member_menu_items(NULL, ids, 3, 1) == 0);

    /* Edit Profile: the fields in and out, Save and the cross. */
    memset(&in, 0, sizeof(in));
    strcpy(in.fullname, "Ada Lovelace");
    strcpy(in.username, "ada");
    strcpy(in.initials, "AL");
    strcpy(in.email, "ada@example.com");
    wena_member_profile_open(&profile, &in);
    assert(profile.visible && profile.focus && profile.error == WENA_WEKAN_PROFILE_SAVED);
    wena_member_profile_read(&profile, &out);
    assert(!strcmp(out.fullname, in.fullname) && !strcmp(out.username, "ada") && !strcmp(out.initials, "AL") &&
           !strcmp(out.email, "ada@example.com"));
    memset(&context, 0, sizeof(context));
    context.edit_text = "Ada King";
    context.button_to_press = "Save";
    assert(wena_member_profile_render(&context, &profile, 1024.0f, 720.0f) == WENA_MEMBER_SAVE);
    wena_member_profile_read(&profile, &out);
    assert(!strcmp(out.fullname, "Ada King") && !strcmp(out.username, "ada") && !profile.focus);
    /* WeKan's errors show in the form. */
    profile.error = WENA_WEKAN_PROFILE_USERNAME_TAKEN;
    assert(wena_member_profile_render(&context, &profile, 1024.0f, 720.0f) == 0);
    context.button_to_press = "Close";
    assert(wena_member_profile_render(&context, &profile, 1024.0f, 720.0f) == WENA_MEMBER_CLOSED && !profile.visible);
    /* Negative: no username, no save; closed draws nothing. */
    wena_member_profile_open(&profile, &in);
    profile.username_length = 0;
    context.button_to_press = "Save";
    assert(wena_member_profile_render(&context, &profile, 1024.0f, 720.0f) == 0);
    profile.visible = 0;
    assert(wena_member_profile_render(&context, &profile, 1024.0f, 720.0f) == 0);
    wena_member_profile_open(&profile, NULL);
    assert(profile.username_length == 0 && profile.visible);

    /* Change Settings: the toggles' fields, a click on one, Save. */
    assert(!strcmp(wena_member_toggle_field(WENA_MEMBER_TOGGLE_DRAG_HANDLES), "showDesktopDragHandles"));
    assert(!strcmp(wena_member_toggle_field(WENA_MEMBER_TOGGLE_SUBMIT_ON_ENTER), "submitOnEnter"));
    assert(!strcmp(wena_member_toggle_field(WENA_MEMBER_TOGGLE_RESCUE_DESCRIPTION), "rescueCardDescription"));
    assert(wena_member_toggle_field(WENA_MEMBER_TOGGLE_COUNT) == NULL);
    wena_member_settings_open(&settings, on, 7);
    assert(settings.visible && settings.on[0] && !settings.on[1] && settings.on[4]);
    assert(wena_member_settings_count(&settings, &count) && count == 7);
    memset(&context, 0, sizeof(context));
    context.button_to_press = "Submit editors with Enter";
    assert(wena_member_settings_render(&context, &settings, 1024.0f, 720.0f) == 2 + WENA_MEMBER_TOGGLE_SUBMIT_ON_ENTER);
    context.button_to_press = "Save";
    assert(wena_member_settings_render(&context, &settings, 1024.0f, 720.0f) == WENA_MEMBER_SAVE);
    /* Negative: a count that is not one (below -1, letters) does not save. */
    strcpy(settings.count, "-2");
    settings.count_length = 2;
    assert(!wena_member_settings_count(&settings, &count));
    context.button_to_press = "Save";
    assert(wena_member_settings_render(&context, &settings, 1024.0f, 720.0f) == 0);
    strcpy(settings.count, "5x");
    settings.count_length = 2;
    assert(!wena_member_settings_count(&settings, &count));
    settings.count_length = 0;
    assert(!wena_member_settings_count(&settings, &count));
    wena_member_settings_open(&settings, NULL, -5);
    assert(wena_member_settings_count(&settings, &count) && count == -1 && !settings.on[0]);
    context.button_to_press = "Close";
    assert(wena_member_settings_render(&context, &settings, 1024.0f, 720.0f) == WENA_MEMBER_CLOSED);
    puts("member settings tests passed");
    return 0;
}
