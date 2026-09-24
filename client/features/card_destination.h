#ifndef WENA_CARD_DESTINATION_H
#define WENA_CARD_DESTINATION_H
#include "directory_picker.h"
/* Reuses one paginated picker for board then active-card selection. The host
 * owns confirmation, cancellation and the selected entity's complete snapshot. */
typedef struct WenaCardDestination {
    WenaDirectoryPicker picker;
    WenaId board_id;
    WenaDirectoryRow card;
    int selected;
} WenaCardDestination;
int wena_card_destination_init(WenaCardDestination*,size_t,WenaDirectoryLoad,void*);
int wena_card_destination_open(WenaCardDestination*,const char *board_id);
void wena_card_destination_close(WenaCardDestination*);
int wena_card_destination_poll(WenaCardDestination*);
int wena_card_destination_render(struct nk_context*,WenaCardDestination*);
#endif
