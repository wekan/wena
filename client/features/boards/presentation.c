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
    wena_checklist_contents_free(view->contents);
    wena_card_sections_free(view->sections);
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
    view->refresh_pending=1;view->summary_pending=1;view->sections_pending=1;
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
    if (valid) valid=wena_checklist_contents_load(
        view->settings_mutation.persistence.database,
        view->settings_mutation.actor_id,view->settings_mutation.board_id,
        &view->contents);
    /* Settings and contents must describe the same board revision. */
    if (valid && settings.board_version!=view->contents->summary.board_version) valid=0;
    if (valid) {
        view->settings=settings;
        if (settings.show_checklist_count) *view->summary=view->contents->summary;
        else {
            memset(view->summary,0,sizeof(*view->summary));
            strcpy(view->summary->board_id,settings.board_id);
        }
    }
    view->summary_valid=valid;view->summary_error=!valid;
    return valid;
}
int wena_board_presentation_sections_refresh(WenaBoardPresentation *view)
{
    if (!initialized(view)) return 0;
    view->sections_pending=0;
    view->sections_valid=wena_card_sections_load(view->mutation.persistence.database,
        view->mutation.actor_id,view->mutation.board_id,&view->sections);
    view->sections_error=!view->sections_valid;return view->sections_valid;
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
        view->sections_valid=0;view->sections_pending=1;
    }
    if (view->refresh_pending) (void)wena_board_presentation_labels_refresh(view);
    if (view->summary_pending) (void)wena_board_presentation_summary_refresh(view);
    if (view->sections_pending) (void)wena_board_presentation_sections_refresh(view);
    return view->valid && view->summary_valid && view->sections_valid;
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
        snapshot->show_checklist_count!=view->settings.show_checklist_count ||
        snapshot->show_checklists!=view->settings.show_checklists)) {
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

int wena_board_presentation_settings_save_display(void *context,const char *board_id,
    unsigned long version,int count,int contents)
{
    WenaBoardPresentation *view;
    view=(WenaBoardPresentation *)context;
    if (!initialized(view) || !wena_board_settings_mutation_save_display(
        &view->settings_mutation,board_id,version,count,contents)) return 0;
    view->summary_valid=0;view->summary_pending=1;return 1;
}
