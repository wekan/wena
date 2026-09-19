#ifndef WENA_BOARD_SETTINGS_STORE_H
#define WENA_BOARD_SETTINGS_STORE_H
#include "../../../models/model.h"
typedef struct WenaBoardSettingsSnapshot {
    WenaId board_id;
    unsigned long board_version;
    int show_checklist_count;
} WenaBoardSettingsSnapshot;
int wena_board_settings_snapshot_valid(const WenaBoardSettingsSnapshot *snapshot,
    const char *board_id);
#endif
