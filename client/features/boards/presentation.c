#include "presentation.h"
#include <string.h>

static int initialized(const WenaBoardPresentation *view)
{
    return view && view->badges && view->summary &&
        view->mutation.persistence.database &&
        view->mutation.persistence.database==view->settings_mutation.persistence.database;
}
void wena_board_presentation_close(WenaBoardPresentation *view)
{
    if (!view) return;
    wena_label_board_snapshot_free(view->badges);
    wena_checklist_summary_free(view->summary);
    memset(view,0,sizeof(*view));
}
int wena_board_presentation_init(WenaBoardPresentation *view,sqlite3 *database,
    const char *actor_id,const char *board_id)
{
    if (!view) return 0;
    memset(view,0,sizeof(*view));
    if (!wena_label_mutation_init(&view->mutation,database,actor_id,board_id) ||
        !wena_board_settings_mutation_init(&view->settings_mutation,database,actor_id,board_id)) {
        wena_board_presentation_close(view);return 0;
    }
    view->badges=wena_label_board_snapshot_create();
    view->summary=wena_checklist_summary_create();
    if (!view->badges || !view->summary) {
        wena_board_presentation_close(view);return 0;
    }
    view->observed_changes=sqlite3_total_changes64(database);
    view->refresh_pending=1;view->summary_pending=1;
    (void)wena_board_presentation_poll(view);
    return 1;
}
int wena_board_presentation_labels_refresh(WenaBoardPresentation *view)
{
    if (!initialized(view)) return 0;
    view->refresh_pending=0;
    view->valid=wena_label_mutation_load_board(&view->mutation,
        view->mutation.board_id,view->badges);
    view->error=!view->valid;
    return view->valid;
}
int wena_board_presentation_summary_refresh(WenaBoardPresentation *view)
{
    WenaBoardSettingsSnapshot settings;
    int valid;
    if (!initialized(view)) return 0;
    view->summary_pending=0;
    valid=wena_board_settings_mutation_load(&view->settings_mutation,
        view->settings_mutation.board_id,&settings);
    if (valid) valid=wena_checklist_summary_load(
        view->settings_mutation.persistence.database,
        view->settings_mutation.actor_id,view->settings_mutation.board_id,
        settings.show_checklist_count,view->summary);
    /* These are separate atomic reads. Do not publish a count projection from
     * a different board revision than the opt-in setting that requested it. */
    if (valid && settings.show_checklist_count &&
        settings.board_version!=view->summary->board_version) valid=0;
    if (valid) view->settings=settings;
    view->summary_valid=valid;view->summary_error=!valid;
    return valid;
}
int wena_board_presentation_poll(WenaBoardPresentation *view)
{
    sqlite3_int64 changes;
    if (!initialized(view)) return 0;
    changes=sqlite3_total_changes64(view->mutation.persistence.database);
    if (changes!=view->observed_changes) {
        view->observed_changes=changes;
        view->valid=0;view->refresh_pending=1;
        view->summary_valid=0;view->summary_pending=1;
    }
    if (view->refresh_pending) (void)wena_board_presentation_labels_refresh(view);
    if (view->summary_pending) (void)wena_board_presentation_summary_refresh(view);
    return view->valid && view->summary_valid;
}
int wena_board_presentation_labels_load(void *context,const char *board_id,
    const char *card_id,WenaLabelSnapshot *snapshot)
{
    WenaBoardPresentation *view;
    int loaded;
    view=(WenaBoardPresentation *)context;
    if (!initialized(view)) return 0;
    loaded=wena_label_mutation_load(&view->mutation,board_id,card_id,snapshot);
    if (loaded && (!view->valid ||
        view->badges->catalogue.board_version!=snapshot->board_version)) {
        view->valid=0;view->refresh_pending=1;
    }
    return loaded;
}
int wena_board_presentation_labels_save(void *context,const char *board_id,
    const char *card_id,const WenaLabelEdit *edit)
{
    WenaBoardPresentation *view;
    view=(WenaBoardPresentation *)context;
    if (!initialized(view) ||
        !wena_label_mutation_save(&view->mutation,board_id,card_id,edit)) return 0;
    view->valid=0;view->refresh_pending=1;
    return 1;
}
int wena_board_presentation_settings_load(void *context,const char *board_id,
    WenaBoardSettingsSnapshot *snapshot)
{
    WenaBoardPresentation *view;
    int loaded;
    view=(WenaBoardPresentation *)context;
    if (!initialized(view)) return 0;
    loaded=wena_board_settings_mutation_load(&view->settings_mutation,board_id,snapshot);
    if (loaded && (!view->summary_valid ||
        snapshot->board_version!=view->settings.board_version ||
        snapshot->show_checklist_count!=view->settings.show_checklist_count)) {
        view->summary_valid=0;view->summary_pending=1;
    }
    return loaded;
}
int wena_board_presentation_settings_save(void *context,const char *board_id,
    unsigned long expected_board_version,int show_checklist_count)
{
    WenaBoardPresentation *view;
    view=(WenaBoardPresentation *)context;
    if (!initialized(view) || !wena_board_settings_mutation_save(
        &view->settings_mutation,board_id,expected_board_version,show_checklist_count)) return 0;
    view->summary_valid=0;view->summary_pending=1;
    return 1;
}
