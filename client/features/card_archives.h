#ifndef WENA_CARD_ARCHIVES_H
#define WENA_CARD_ARCHIVES_H
#include "card_details.h"
#include "../components/common/paginated_table.h"
#include "../components/boards/board_layout.h"

typedef int (*WenaCardArchivesRestore)(void *context, const char *board_id,
    const char *card_id, unsigned long expected_version);

typedef struct WenaCardArchivesState {
    WenaTableState table;
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
void wena_card_archives_close(WenaCardArchivesState *state);
int wena_card_archives_open(WenaCardArchivesState *state,
    const WenaBoardLayout *layout);
int wena_card_archives_render(struct nk_context *context,
    WenaCardArchivesState *state, const WenaBoardLayout *layout,
    float width, float height);
#endif
