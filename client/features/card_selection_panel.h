#ifndef WENA_CARD_SELECTION_PANEL_H
#define WENA_CARD_SELECTION_PANEL_H
#include "../../models/card_selection.h"
#include "../components/common/paginated_table.h"
typedef struct WenaCardSelectionPanel {
    WenaCardSelection *selection;
    WenaTableState table;
    WenaId list_id,lane_id;
    int visible,error;
} WenaCardSelectionPanel;
/* Non-owning selection storage must outlive the panel. Opening unions the
 * scoped active cards into selection; failed opens preserve both objects. */
void wena_card_selection_panel_init(WenaCardSelectionPanel *panel,WenaCardSelection *selection);
int wena_card_selection_panel_open(WenaCardSelectionPanel *panel,const WenaCard *cards,
    size_t count,const char *board,const char *list,const char *lane);
/* Closing/disabling selection clears its IDs. */
void wena_card_selection_panel_close(WenaCardSelectionPanel *panel);
int wena_card_selection_panel_render(struct nk_context *context,WenaCardSelectionPanel *panel,
    const WenaCard *cards,size_t count,const char *board,float width,float height);
/* Reusable cached minicard checkbox; returns a card-body intent, never writes. */
unsigned int wena_card_selection_control(struct nk_context *context,void *selection,
    const WenaCard *card);
#endif
