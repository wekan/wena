#ifndef WENA_BOARD_SETTINGS_PANEL_H
#define WENA_BOARD_SETTINGS_PANEL_H

#include "settings_store.h"

struct nk_context;
typedef int (*WenaBoardSettingsLoad)(void *context, const char *board_id,
    WenaBoardSettingsSnapshot *snapshot);
typedef int (*WenaBoardSettingsSave)(void *context, const char *board_id,
    unsigned long expected_board_version, int show_checklist_count, int show_checklists);

typedef int (*WenaBoardSettingsSaveAll)(void *context, const char *board_id,
    unsigned long version, int count, int contents, int collapse);

typedef struct WenaBoardSettingsState {
    int visible;
    int error;
    int needs_refresh;
    int show_checklist_count;
    int show_checklists;
    int allow_minicard_collapse;
    WenaId board_id;
    WenaBoardSettingsSnapshot snapshot;
    WenaBoardSettingsLoad load;
    WenaBoardSettingsSave save;
    WenaBoardSettingsSaveAll save_all;
    void *context;
} WenaBoardSettingsState;

/* Checkbox changes only a local draft. Save commits an explicit boolean with
 * the loaded board revision; Cancel/Escape closes without a write. NULL save
 * opens read-only inspection. A failed reload after a commit permits Refresh
 * only, preventing the accepted operation from being submitted again. */
void wena_board_settings_init(WenaBoardSettingsState *state,
    WenaBoardSettingsLoad load, WenaBoardSettingsSave save, void *context);
void wena_board_settings_close(WenaBoardSettingsState *state);
int wena_board_settings_open(WenaBoardSettingsState *state, const char *board_id);
int wena_board_settings_render(struct nk_context *context,
    WenaBoardSettingsState *state, const char *board_id, float width, float height);

#endif
