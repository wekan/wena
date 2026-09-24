#ifndef WENA_CARD_ARCHIVES_H
#define WENA_CARD_ARCHIVES_H
#include "card_details.h"
#include "../components/common/paginated_table.h"
#include "../components/boards/board_layout.h"

typedef int (*WenaCardArchivesRestore)(void *context, const char *board_id,
    const char *card_id, unsigned long expected_version);

typedef int (*WenaArchivesLoadVersion)(void *context,const char *board_id,
    const char *item_id,unsigned long *version);

typedef enum WenaArchiveKind {
    WENA_ARCHIVE_CARDS, WENA_ARCHIVE_LISTS, WENA_ARCHIVE_SWIMLANES,
    WENA_ARCHIVE_KIND_COUNT
} WenaArchiveKind;
typedef struct WenaArchiveProvider {
    WenaArchivesLoadVersion load;
    WenaCardArchivesRestore restore;
    void *context;
} WenaArchiveProvider;
typedef struct WenaCardArchivesState {
    WenaTableState table;
    WenaArchiveKind kind;
    WenaArchiveProvider providers[WENA_ARCHIVE_KIND_COUNT];
    int visible;
    int error;
    WenaId board_id;
    WenaId card_id;
    unsigned long version;
    WenaCardDetailsLoadTitle load;
    WenaCardArchivesRestore restore;
    void *context;
} WenaCardArchivesState;

void wena_card_archives_init(WenaCardArchivesState *state,
    WenaCardDetailsLoadTitle load, WenaCardArchivesRestore restore, void *context);
/* Optional list provider shares the same paginated selection and restore flow. */
void wena_card_archives_set_lists(WenaCardArchivesState *state,
    WenaArchivesLoadVersion load,WenaCardArchivesRestore restore,void *context);
/* Hierarchy categories share one table and version/restore provider contract. */
void wena_card_archives_set_provider(WenaCardArchivesState *state,
    WenaArchiveKind kind,WenaArchivesLoadVersion load,
    WenaCardArchivesRestore restore,void *context);
void wena_card_archives_close(WenaCardArchivesState *state);
int wena_card_archives_open(WenaCardArchivesState *state,
    const WenaBoardLayout *layout);
int wena_card_archives_render(struct nk_context *context,
    WenaCardArchivesState *state, const WenaBoardLayout *layout,
    float width, float height);
#endif
