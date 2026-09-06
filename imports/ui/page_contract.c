#include "page_contract.h"

static const WenaUiControlContract controls[] = {
    {WENA_UI_BOARD_MENU, "board", "Board menu", "[>]", "GET", "open-board-menu", 1u},
    {WENA_UI_ADD_CARD, "add-card", "Add card", "[+]", "POST", "create-card", 2u},
    {WENA_UI_LIST_MENU, "list", "List menu", "[>]", "GET", "open-list-menu", 3u},
    {WENA_UI_OPEN_CARD, "card", "Open card", "[>]", "GET", "open-card", 4u},
    {WENA_UI_CARD_MENU, "card", "Card menu", "[>]", "GET", "open-card-menu", 5u},
    {WENA_UI_EDIT_TITLE, "edit", "Edit title", "[E]", "POST", "edit-card-title", 6u},
    {WENA_UI_ARCHIVE_CARD, "archive-card", "Archive card", "[A]", "POST", "archive-card", 7u},
    {WENA_UI_CLOSE, "close", "Close details", "[X]", "GET", "close-card", 8u}
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
    return control == NULL ? "" : control->fallback_text;
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
