#include "settings_store.h"
#include <string.h>
int wena_board_settings_snapshot_valid(const WenaBoardSettingsSnapshot *snapshot,
    const char *board_id)
{
    return snapshot && wena_model_identifier_valid(board_id) &&
        wena_model_identifier_valid(snapshot->board_id) &&
        !strcmp(snapshot->board_id,board_id) && snapshot->board_version &&
        snapshot->board_version<=WENA_VERSION_READ_MAX &&
        (snapshot->show_checklist_count==0 || snapshot->show_checklist_count==1) &&
        (snapshot->show_checklists==0 || snapshot->show_checklists==1);
}
