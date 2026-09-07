#ifndef WENA_UI_PAGE_CONTRACT_H
#define WENA_UI_PAGE_CONTRACT_H

#include <stddef.h>

#define WENA_UI_BASELINE_CONTENT_TABLES 1u
#define WENA_UI_NATURAL_TAB_ORDER 1u

typedef enum WenaUiControlId {
    WENA_UI_BOARD_MENU,
    WENA_UI_ADD_CARD,
    WENA_UI_LIST_MENU,
    WENA_UI_OPEN_CARD,
    WENA_UI_CARD_MENU,
    WENA_UI_EDIT_TITLE,
    WENA_UI_ARCHIVE_CARD,
    WENA_UI_CLOSE,
    WENA_UI_MOVE_CARD,
    WENA_UI_MOVE_LIST,
    WENA_UI_MOVE_SWIMLANE,
    WENA_UI_SAVE,
    WENA_UI_CANCEL,
    WENA_UI_COLLAPSE_LIST,
    WENA_UI_EXPAND_LIST,
    WENA_UI_COLLAPSE_SWIMLANE,
    WENA_UI_EXPAND_SWIMLANE,
    WENA_UI_MOVE_CARD_TO,
    WENA_UI_ADD_LIST,
    WENA_UI_ADD_SWIMLANE,
    WENA_UI_RENAME_BOARD,
    WENA_UI_RENAME_SWIMLANE,
    WENA_UI_RESTORE_CARD
} WenaUiControlId;

typedef struct WenaUiControlContract {
    WenaUiControlId id;
    const char *i18n_key;
    const char *fallback_text;
    const char *ascii_control;
    const char *http_method;
    const char *domain_operation;
    unsigned int tab_order;
} WenaUiControlContract;

typedef struct WenaUiPageContract {
    const char *route_family;
    const char *heading_i18n_key;
} WenaUiPageContract;

typedef struct WenaUiColorContract {
    const char *name;
    const char *rgb;
} WenaUiColorContract;

typedef enum WenaUiTextId {
    WENA_UI_TEXT_ACTIVITIES,
    WENA_UI_TEXT_MEMBERS,
    WENA_UI_TEXT_LABELS,
    WENA_UI_TEXT_ARCHIVES,
    WENA_UI_TEXT_REFRESH,
    WENA_UI_TEXT_ADD_MEMBER,
    WENA_UI_TEXT_ADD_LABEL,
    WENA_UI_TEXT_RESTORE,
    WENA_UI_TEXT_LANGUAGE,
    WENA_UI_TEXT_SWIMLANE,
    WENA_UI_TEXT_LIST,
    WENA_UI_TEXT_NO_ARCHIVED_CARDS,
    WENA_UI_TEXT_ERROR
} WenaUiTextId;

/* The renderer owns this process-local callback and its context lifetime.
 * Returned strings must remain alive through the rendered frame. NULL resets
 * the adapter; unknown translations fall back to the existing UI contract. */
typedef const char *(*WenaUiTranslator)(void *context, const char *key);
void wena_ui_set_translator(WenaUiTranslator translator, void *context);
const char *wena_ui_text(WenaUiTextId id);

const WenaUiControlContract *wena_ui_control(WenaUiControlId id);
const char *wena_ui_control_text(WenaUiControlId id);
const WenaUiPageContract *wena_ui_pages(size_t *count);
const WenaUiColorContract *wena_ui_colors(size_t *count);

#endif
