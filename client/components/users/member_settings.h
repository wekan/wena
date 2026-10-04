#ifndef WENA_MEMBER_SETTINGS_H
#define WENA_MEMBER_SETTINGS_H
/* WeKan's member menu (userHeader.jade memberMenuPopup) and the popups of
 * it that change the user: Edit Profile (editProfilePopup) and Change
 * Settings (changeSettingsPopup). What they read and write is the user's
 * WeKan document, through server/wekan_sync.h; these only draw and edit. */
#include <stddef.h>
#include "../common/wekan_look.h"
#include "../../../server/wekan_sync.h"

struct nk_context;

/* The member menu's entries, in WeKan's order. */
typedef enum WenaMemberItem {
    WENA_MEMBER_MY_CARDS,
    WENA_MEMBER_DUE_CARDS,
    WENA_MEMBER_MY_ATTACHMENTS,
    WENA_MEMBER_STARRED,
    WENA_MEMBER_GLOBAL_SEARCH,
    WENA_MEMBER_ALL_BOARDS,
    WENA_MEMBER_PUBLIC,
    WENA_MEMBER_ARCHIVES,
    WENA_MEMBER_TEMPLATES,
    WENA_MEMBER_ADMIN_PANEL,      /* only for an admin */
    WENA_MEMBER_EDIT_PROFILE,
    WENA_MEMBER_DATE,
    WENA_MEMBER_CHANGE_SETTINGS,
    WENA_MEMBER_CHANGE_COLOR,
    WENA_MEMBER_NOTIFICATIONS,
    WENA_MEMBER_CHANGE_FONT,
    WENA_MEMBER_CHANGE_AVATAR,
    WENA_MEMBER_CHANGE_PASSWORD,
    WENA_MEMBER_CHANGE_LANGUAGE,
    WENA_MEMBER_LOG_OUT,
    WENA_MEMBER_ITEM_COUNT
} WenaMemberItem;

/* The menu for this user: items[i] is entry ids[i]; Admin Panel only when
 * `is_admin`. Entries Wena does not have yet are shown, as WeKan has them,
 * but disabled. Returns the count. */
size_t wena_member_menu_items(WenaWekanMenuItem *items, WenaMemberItem *ids, size_t capacity, int is_admin);
/* Whether Wena does this entry. */
int wena_member_item_enabled(WenaMemberItem item);

/* Edit Profile: the form's fields, the result of the last save, and the
 * popup. Render returns WENA_MEMBER_SAVE when Save is pressed with a
 * username, WENA_MEMBER_CLOSED when closed, else 0. */
#define WENA_MEMBER_SAVE 1
#define WENA_MEMBER_CLOSED (-1)
#define WENA_MEMBER_FIELD 256
typedef struct WenaMemberProfileForm {
    int visible, focus;
    char fullname[WENA_MEMBER_FIELD], username[WENA_MEMBER_FIELD];
    char initials[WENA_MEMBER_FIELD], email[WENA_MEMBER_FIELD];
    int fullname_length, username_length, initials_length, email_length;
    int error;                    /* a WENA_WEKAN_PROFILE_* from the last save */
} WenaMemberProfileForm;
void wena_member_profile_open(WenaMemberProfileForm *form, const WenaWekanProfile *profile);
/* The form as the profile to save (lengths ended, fields trimmed of nothing). */
void wena_member_profile_read(const WenaMemberProfileForm *form, WenaWekanProfile *profile);
int wena_member_profile_render(struct nk_context *context, WenaMemberProfileForm *form, float width, float height);

/* Change Settings: WeKan's toggles and the card count. Render returns
 * WENA_MEMBER_SAVE on Save, a toggle's WENA_MEMBER_TOGGLE_* + 2 when one is
 * clicked (written at once, as WeKan's), WENA_MEMBER_CLOSED, or 0. */
typedef enum WenaMemberToggle {
    WENA_MEMBER_TOGGLE_DRAG_HANDLES,
    WENA_MEMBER_TOGGLE_SUBMIT_ON_ENTER,
    WENA_MEMBER_TOGGLE_OPEN_MANY,
    WENA_MEMBER_TOGGLE_CHECKLIST_DING,
    WENA_MEMBER_TOGGLE_RESCUE_DESCRIPTION,
    WENA_MEMBER_TOGGLE_COUNT
} WenaMemberToggle;
/* The profile field each toggle is (profile.<field>). */
const char *wena_member_toggle_field(WenaMemberToggle toggle);
typedef struct WenaMemberSettingsForm {
    int visible;
    int on[WENA_MEMBER_TOGGLE_COUNT];
    char count[16];
    int count_length;
} WenaMemberSettingsForm;
void wena_member_settings_open(WenaMemberSettingsForm *form, const int on[WENA_MEMBER_TOGGLE_COUNT], long count);
/* The count field as a number: 1 with it, 0 when it is not one (-1 and up). */
int wena_member_settings_count(const WenaMemberSettingsForm *form, long *count);
int wena_member_settings_render(struct nk_context *context, WenaMemberSettingsForm *form, float width, float height);

#endif
