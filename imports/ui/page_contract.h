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
    WENA_UI_CLOSE
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

const WenaUiControlContract *wena_ui_control(WenaUiControlId id);
const char *wena_ui_control_text(WenaUiControlId id);
const WenaUiPageContract *wena_ui_pages(size_t *count);
const WenaUiColorContract *wena_ui_colors(size_t *count);

#endif
