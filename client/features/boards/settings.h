#ifndef WENA_BOARD_SETTINGS_H
#define WENA_BOARD_SETTINGS_H
#include "settings_store.h"
#include "../../../server/sqlite_persistence.h"

typedef struct WenaBoardSettingsMutation {
    WenaSqlitePersistence persistence;
    WenaId actor_id;
    WenaId board_id;
    char route[257];
} WenaBoardSettingsMutation;
int wena_board_settings_mutation_init(WenaBoardSettingsMutation *adapter,
    sqlite3 *database,const char *actor_id,const char *board_id);
/* Atomic output: compact count defaults false, expanded contents true. */
int wena_board_settings_mutation_load(void *context,const char *board_id,
    WenaBoardSettingsSnapshot *snapshot);
/* Both boolean states are explicit. Guarded no-op succeeds without metadata or
 * version writes; a change increments board.version exactly once. */
int wena_board_settings_mutation_save(void *context,const char *board_id,
    unsigned long expected_board_version,int show_checklist_count);
int wena_board_settings_mutation_save_request(WenaBoardSettingsMutation *adapter,
    const char *board_id,unsigned long expected_board_version,
    int show_checklist_count,unsigned long request_version);
/* Save both preferences in one transaction and advance the board only once. */
int wena_board_settings_mutation_save_display(void *context,const char *board_id,
    unsigned long version,int count,int contents);
int wena_board_settings_mutation_save_display_request(WenaBoardSettingsMutation *adapter,
    const char *board_id,unsigned long version,int count,int contents,unsigned long request);
#endif
