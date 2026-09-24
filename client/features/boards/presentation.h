#ifndef WENA_BOARD_PRESENTATION_H
#define WENA_BOARD_PRESENTATION_H
#include "settings.h"
#include "../labels/mutation.h"
#include "../checklists/summary.h"

typedef struct WenaBoardPresentation {
    WenaLabelMutation mutation;
    WenaLabelBoardSnapshot *badges;
    int valid;
    int refresh_pending;
    int error;
    WenaBoardSettingsMutation settings_mutation;
    WenaBoardSettingsSnapshot settings;
    WenaChecklistBoardSummary *summary;
    WenaChecklistBoardContents *contents;
    int summary_valid;
    int summary_pending;
    int summary_error;
    sqlite3_int64 observed_changes;
} WenaBoardPresentation;
/* Initialize fresh or closed storage. Owns snapshots, never the database.
 * A successful allocation/init returns 1; initial read failures are represented
 * by the valid/error flags, so callers can display an explicit read-only retry. */
int wena_board_presentation_init(WenaBoardPresentation *view,sqlite3 *database,
    const char *actor_id,const char *board_id);
void wena_board_presentation_close(WenaBoardPresentation *view);
/* Call once after editor processing. A cheap connection-local change counter
 * invalidates caches after writes (including harmless rolled-back changes).
 * Unchanged frames issue no SQL. Each pending read is attempted only once;
 * failures require another write, panel open or explicit pending-flag retry. */
int wena_board_presentation_poll(WenaBoardPresentation *view);
int wena_board_presentation_labels_refresh(WenaBoardPresentation *view);
int wena_board_presentation_summary_refresh(WenaBoardPresentation *view);
/* Panel callbacks preserve committed-save success even if later cache loading
 * fails. Known stale data is hidden immediately via the validity flags. */
int wena_board_presentation_labels_load(void *context,const char *board_id,
    const char *card_id,WenaLabelSnapshot *snapshot);
int wena_board_presentation_labels_save(void *context,const char *board_id,
    const char *card_id,const WenaLabelEdit *edit);
int wena_board_presentation_settings_load(void *context,const char *board_id,
    WenaBoardSettingsSnapshot *snapshot);
int wena_board_presentation_settings_save(void *context,const char *board_id,
    unsigned long expected_board_version,int show_checklist_count);
#endif
