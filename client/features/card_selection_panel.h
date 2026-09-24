#ifndef WENA_CARD_SELECTION_PANEL_H
#define WENA_CARD_SELECTION_PANEL_H
#include "../../models/card_selection.h"
#include "../../models/card_revision.h"
#include "../components/common/paginated_table.h"
typedef int (*WenaSelectionCapture)(void *,const char *,const WenaId *,size_t,WenaCardRevision **);
typedef int (*WenaSelectionArchive)(void *,const char *,const WenaCardRevision *,size_t);
typedef struct WenaCardSelectionPanel {
    WenaCardSelection *selection;
    WenaTableState table;
    WenaId list_id,lane_id;
    int visible,error,archive_error;
    WenaCardRevision *captured;
    size_t captured_count;
    WenaSelectionCapture capture;
    WenaSelectionArchive archive;
    void *archive_context;
} WenaCardSelectionPanel;
/* Non-owning selection storage must outlive the panel. Opening unions the
 * scoped active cards into selection; failed opens preserve both objects. */
void wena_card_selection_panel_init(WenaCardSelectionPanel *panel,WenaCardSelection *selection);
int wena_card_selection_panel_open(WenaCardSelectionPanel *panel,const WenaCard *cards,
    size_t count,const char *board,const char *list,const char *lane);
/* Hiding cancels confirmation but retains selected IDs; disabling clears IDs. */
void wena_card_selection_panel_hide(WenaCardSelectionPanel *panel);
void wena_card_selection_panel_set_archive(WenaCardSelectionPanel *panel,
    WenaSelectionCapture capture,WenaSelectionArchive archive,void *context);
/* Closing/disabling selection clears its IDs. */
void wena_card_selection_panel_close(WenaCardSelectionPanel *panel);
int wena_card_selection_panel_render(struct nk_context *context,WenaCardSelectionPanel *panel,
    const WenaCard *cards,size_t count,const char *board,float width,float height);
/* Zero-initialize caller-owned traversal storage once. Begin before drawing,
 * visit controls in
 * display order, then apply an intent after drawing. Hidden anchors fall back
 * to a single toggle; a valid range keeps its anchor for repeated Shift clicks.
 * Duplicate/overflow traversal fails closed without changing selected IDs. */
typedef struct WenaCardSelectionTraversal {
    WenaCardSelection *selection;
    WenaId board_id,anchor,ids[WENA_CARD_SELECTION_CAPACITY];
    size_t count;
    int error;
} WenaCardSelectionTraversal;
void wena_card_selection_traversal_begin(WenaCardSelectionTraversal *state,WenaCardSelection *selection);
unsigned int wena_card_selection_traversal_control(struct nk_context *context,void *state,const WenaCard *card);
int wena_card_selection_traversal_apply(WenaCardSelectionTraversal *state,const WenaCard *cards,
    size_t count,const char *target,unsigned int action);
/* Reusable cached minicard checkbox; returns a card-body intent, never writes. */
unsigned int wena_card_selection_control(struct nk_context *context,void *selection,
    const WenaCard *card);
#endif
