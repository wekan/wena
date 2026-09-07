#include "page_contract.h"

static WenaUiTranslator translation_callback;
static void *translation_context;

typedef struct WenaUiTextContract {
    WenaUiTextId id;
    const char *i18n_key;
    const char *fallback_text;
} WenaUiTextContract;

static const WenaUiTextContract texts[] = {
    {WENA_UI_TEXT_ACTIVITIES, "activities", "Activities"},
    {WENA_UI_TEXT_MEMBERS, "members", "Members"},
    {WENA_UI_TEXT_LABELS, "labels", "Labels"},
    {WENA_UI_TEXT_ARCHIVES, "archives", "Archives"},
    {WENA_UI_TEXT_REFRESH, "refresh", "Refresh"},
    {WENA_UI_TEXT_ADD_MEMBER, "add-members", "Add member"},
    {WENA_UI_TEXT_ADD_LABEL, "add-label", "Add label"},
    {WENA_UI_TEXT_RESTORE, "restore", "Restore selected"},
    {WENA_UI_TEXT_LANGUAGE, "language", "Language"},
    {WENA_UI_TEXT_SWIMLANE, "swimlane", "Swimlane"},
    {WENA_UI_TEXT_LIST, "list", "List"},
    {WENA_UI_TEXT_NO_ARCHIVED_CARDS, "no-archived-cards", "No archived cards"},
    {WENA_UI_TEXT_ERROR, "error", "Error"}
};

void wena_ui_set_translator(WenaUiTranslator translator, void *context)
{
    translation_callback = translator;
    translation_context = translator == NULL ? NULL : context;
}

static const char *translated(const char *key, const char *fallback)
{
    const char *value;
    if (translation_callback != NULL) {
        value = translation_callback(translation_context, key);
        if (value != NULL && value[0] != '\0') return value;
    }
    return fallback;
}

const char *wena_ui_text(WenaUiTextId id)
{
    size_t i;
    for (i = 0; i < sizeof(texts) / sizeof(texts[0]); ++i)
        if (texts[i].id == id)
            return translated(texts[i].i18n_key, texts[i].fallback_text);
    return "";
}

static const WenaUiControlContract controls[] = {
    {WENA_UI_BOARD_MENU, "board", "Board menu", "[>]", "GET", "open-board-menu", 1u},
    {WENA_UI_ADD_CARD, "add-card", "Add card", "[+]", "POST", "create-card", 2u},
    {WENA_UI_LIST_MENU, "list", "List menu", "[>]", "GET", "open-list-menu", 3u},
    {WENA_UI_OPEN_CARD, "minicardDetailsActionsPopup-title", "Open card", "[>]", "GET", "open-card", 4u},
    {WENA_UI_CARD_MENU, "cardDetailsActionsPopup-title", "Card menu", "[>]", "GET", "open-card-menu", 5u},
    {WENA_UI_EDIT_TITLE, "edit", "Edit title", "[E]", "POST", "edit-card-title", 6u},
    {WENA_UI_ARCHIVE_CARD, "archive-card", "Archive card", "[A]", "POST", "archive-card", 7u},
    {WENA_UI_CLOSE, "close", "Close details", "[X]", "GET", "close-card", 8u},
    {WENA_UI_MOVE_CARD, "move-card-up", "Move card", "[>]", "POST", "move-card", 9u},
    {WENA_UI_MOVE_LIST, "move-list-right", "Move list", "[>]", "POST", "move-list", 10u},
    {WENA_UI_MOVE_SWIMLANE, "move-swimlane", "Move swimlane", "[>]", "POST", "move-swimlane", 11u},
    {WENA_UI_SAVE, "save", "Save", "[S]", "GET", "save-card-editor", 12u},
    {WENA_UI_CANCEL, "cancel", "Cancel", "[X]", "GET", "cancel-card-editor", 13u},
    {WENA_UI_COLLAPSE_LIST, "collapse", "Collapse", "[-]", "GET", "collapse-list", 14u},
    {WENA_UI_EXPAND_LIST, "uncollapse", "Uncollapse", "[+]", "GET", "expand-list", 15u},
    {WENA_UI_COLLAPSE_SWIMLANE, "collapse", "Collapse", "[-]", "GET", "collapse-swimlane", 16u},
    {WENA_UI_EXPAND_SWIMLANE, "uncollapse", "Uncollapse", "[+]", "GET", "expand-swimlane", 17u},
    {WENA_UI_MOVE_CARD_TO, "moveCardPopup-title", "Move card", "[>]", "GET", "open-move-card", 18u},
    {WENA_UI_ADD_LIST, "add-list", "Add list", "[+]", "GET", "open-create-list", 19u},
    {WENA_UI_ADD_SWIMLANE, "add-swimlane", "Add swimlane", "[+]", "GET", "open-create-swimlane", 20u},
    {WENA_UI_RENAME_BOARD, "rename", "Rename board", "[E]", "GET", "open-rename-board", 21u},
    {WENA_UI_RENAME_SWIMLANE, "rename", "Rename swimlane", "[E]", "GET", "open-rename-swimlane", 22u},
    {WENA_UI_RESTORE_CARD, "restore", "Restore", "[R]", "GET", "restore-card-editor", 23u}
};

static const WenaUiPageContract pages[] = {
    {"/", "loginPopup-title"}, {"/sign-in", "loginPopup-title"},
    {"/sign-up", "signupPopup-title"}, {"/allboards", "all-boards"},
    {"/public", "public"}, {"/my-cards", "my-cards"},
    {"/due-cards", "dueCards-title"}, {"/global-search", "globalSearch-title"},
    {"/bookmarks", "bookmarksPopup-title"}, {"/import", "import"},
    {"/support", "support"}, {"/accessibility", "accessibility"},
    {"/shortcuts", "keyboard-shortcuts"}, {"/admin", "admin-panel"},
    {"/b/:boardId/:slug", "board"}
};

static const WenaUiColorContract colors[] = {
    {"belize", "#2980b9"}, {"nephritis", "#27ae60"},
    {"pomegranate", "#c0392b"}, {"pumpkin", "#e67e22"},
    {"wisteria", "#8e44ad"}, {"moderatepink", "#cd5a91"},
    {"strongcyan", "#00aecc"}, {"limegreen", "#4bbf6b"},
    {"midnight", "#2c3e50"}, {"dark", "#333333"},
    {"relax", "#568ba2"}, {"corteza", "#568ba2"},
    {"natural", "#6b8e23"}, {"modern", "#2980b9"},
    {"moderndark", "#263238"}, {"exodark", "#1f2933"},
    {"cleandark", "#263238"}, {"cleanlight", "#e8f3fa"},
    {"clearblue", "#2980b9"}, {"cleargreen", "#27ae60"},
    {"clearorange", "#e67e22"}, {"clearpink", "#cd5a91"},
    {"clearpurple", "#8e44ad"}, {"clearred", "#c0392b"},
    {"appleglasspastel", "#568ba2"},
    {"white", "#ffffff"}, {"green", "#3cb500"},
    {"yellow", "#fad900"}, {"orange", "#ff9f19"},
    {"red", "#eb4646"}, {"purple", "#a632db"},
    {"blue", "#0079bf"}, {"sky", "#00c2e0"},
    {"lime", "#51e898"}, {"pink", "#ff78cb"},
    {"black", "#4d4d4d"}, {"silver", "#c0c0c0"},
    {"peachpuff", "#ffdab9"}, {"crimson", "#dc143c"},
    {"plum", "#dda0dd"}, {"darkgreen", "#006400"},
    {"slateblue", "#6a5acd"}, {"magenta", "#ff00ff"},
    {"gold", "#ffd700"}, {"navy", "#000080"},
    {"gray", "#808080"}, {"saddlebrown", "#8b4513"},
    {"paleturquoise", "#afeeee"}, {"mistyrose", "#ffe4e1"},
    {"indigo", "#4b0082"}
};

const WenaUiControlContract *wena_ui_control(WenaUiControlId id)
{
    size_t index;
    for (index = 0; index < sizeof(controls) / sizeof(controls[0]); ++index) {
        if (controls[index].id == id) {
            return &controls[index];
        }
    }
    return NULL;
}

const char *wena_ui_control_text(WenaUiControlId id)
{
    const WenaUiControlContract *control;
    control = wena_ui_control(id);
    return control == NULL ? "" :
        translated(control->i18n_key, control->fallback_text);
}

const WenaUiPageContract *wena_ui_pages(size_t *count)
{
    if (count != NULL) {
        *count = sizeof(pages) / sizeof(pages[0]);
    }
    return pages;
}

const WenaUiColorContract *wena_ui_colors(size_t *count)
{
    if (count != NULL) {
        *count = sizeof(colors) / sizeof(colors[0]);
    }
    return colors;
}
