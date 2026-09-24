#ifndef WENA_HIERARCHY_MUTATION_H
#define WENA_HIERARCHY_MUTATION_H

#include "hierarchy_title.h"
#include "../../models/card_move_selection.h"
#include "../../server/sqlite_board.h"
#include "../../server/sqlite_persistence.h"

typedef struct WenaHierarchyMutation {
    WenaSqlitePersistence persistence;
    WenaId actor_id;
    WenaId board_id;
    char route[257];
    WenaSqliteBoardSnapshot *snapshot;
    /* Optional peer traversal count, updated with full snapshot publication.
     * Caller-owned storage must outlive this adapter. */
    size_t *published_card_count;
} WenaHierarchyMutation;

/* Caller authenticates actor and authorizes board. No implicit membership grant.
 * The database and caller-owned loader snapshot must outlive this adapter. */
int wena_hierarchy_mutation_init(WenaHierarchyMutation *adapter, sqlite3 *database,
    const char *actor_id, const char *board_id, WenaSqliteBoardSnapshot *snapshot);
int wena_hierarchy_mutation_load(void *context, const char *board_id,
    WenaHierarchyKind kind, const char *target_id, char *title,
    size_t capacity, unsigned long *version);
int wena_hierarchy_mutation_save(void *context, const char *board_id,
    WenaHierarchyKind kind, const char *target_id, unsigned long expected_version,
    const char *title);
/* A committed replay returns 0. Failed requests leave display models unchanged. */
int wena_hierarchy_mutation_save_request(WenaHierarchyMutation *adapter,
    const char *board_id, WenaHierarchyKind kind, const char *target_id,
    unsigned long expected_version, unsigned long request_version, const char *title);
/* List/swimlane creation preflights snapshot capacity and publishes the exact
 * committed generated ID/position without a fallible post-commit reload. */
int wena_hierarchy_mutation_create(void *context, const char *board_id,
    WenaHierarchyKind kind, const char *title);
int wena_hierarchy_mutation_create_request(WenaHierarchyMutation *adapter,
    const char *board_id, WenaHierarchyKind kind, unsigned long request_version,
    const char *title);
/* Ordinary-list archive controls reuse this adapter and its complete snapshot.
 * Failed loads preserve version; failed writes preserve the entire snapshot. */
int wena_hierarchy_mutation_archive_load(void *context,const char *board_id,
    const char *list_id,unsigned long *version);
int wena_hierarchy_mutation_archive(void *context,const char *board_id,
    const char *list_id,unsigned long expected_version);
int wena_hierarchy_mutation_restore(void *context,const char *board_id,
    const char *list_id,unsigned long expected_version);
int wena_hierarchy_mutation_archive_request(WenaHierarchyMutation *adapter,
    const char *board_id,const char *list_id,unsigned long expected_version,
    unsigned long request_version,int archived);
/* Shared list/swimlane color callbacks; loads and saves publish atomically. */
int wena_hierarchy_mutation_color_load(void *context,const char *board_id,
    WenaHierarchyKind kind,const char *target_id,char *color,size_t capacity,unsigned long *version);
int wena_hierarchy_mutation_color_save(void *context,const char *board_id,
    WenaHierarchyKind kind,const char *target_id,unsigned long expected_version,const char *color);
int wena_hierarchy_mutation_color_save_request(WenaHierarchyMutation *adapter,
    const char *board_id,WenaHierarchyKind kind,const char *target_id,
    unsigned long expected_version,unsigned long request_version,const char *color);
/* WIP loads return one read snapshot of settings, active count and revision.
 * Writes publish only exact committed settings; failed outputs/cache survive. */
int wena_hierarchy_mutation_wip_load(void *context,const char *board_id,const char *list_id,
    WenaWipLimit *limit,size_t *count,unsigned long *version);
int wena_hierarchy_mutation_wip_save(void *context,const char *board_id,const char *list_id,
    unsigned long expected_version,WenaWipEdit edit,size_t value);
int wena_hierarchy_mutation_wip_save_request(WenaHierarchyMutation *adapter,
    const char *board_id,const char *list_id,unsigned long expected_version,
    unsigned long request_version,WenaWipEdit edit,size_t value);
/* Lane cascades stage a complete authoritative board before commit and publish
 * it after success. Any staging/commit failure leaves the entire cache intact. */
int wena_hierarchy_mutation_swimlane_archive_load(void *context,const char *board_id,
    const char *swimlane_id,unsigned long *version);
int wena_hierarchy_mutation_swimlane_archive(void *context,const char *board_id,
    const char *swimlane_id,unsigned long expected_version);
int wena_hierarchy_mutation_swimlane_restore(void *context,const char *board_id,
    const char *swimlane_id,unsigned long expected_version);
int wena_hierarchy_mutation_swimlane_archive_request(WenaHierarchyMutation *adapter,
    const char *board_id,const char *swimlane_id,unsigned long expected_version,
    unsigned long request_version,int archived);
/* Empty/NULL lane selects the whole list; otherwise capture both revisions.
 * Loads preserve outputs on failure; writes share full snapshot staging. */
int wena_hierarchy_mutation_list_cards_load(void *context,const char *board,
    const char *list,const char *lane,unsigned long *list_version,unsigned long *lane_version);
int wena_hierarchy_mutation_list_cards_archive_request(WenaHierarchyMutation *adapter,
    const char *board,const char *list,const char *lane,unsigned long expected,
    unsigned long lane_version,unsigned long request);
int wena_hierarchy_mutation_list_cards_archive(void *context,const char *board,
    const char *list,const char *lane,unsigned long expected,unsigned long lane_version);
/* Capture one read snapshot of exact selected IDs/revisions. Initialize *output
 * to NULL; successful load replaces/frees prior storage, failure preserves it.
 * Caller frees captured rows. Writes never modify the captured selection. */
int wena_hierarchy_mutation_selected_cards_load(void *context,const char *board,
    const WenaId *ids,size_t count,WenaDomainCardRevision **output);
int wena_hierarchy_mutation_selected_cards_archive_request(WenaHierarchyMutation *adapter,
    const char *board,const WenaDomainCardRevision *cards,size_t count,unsigned long request);
int wena_hierarchy_mutation_selected_cards_archive(void *context,const char *board,
    const WenaDomainCardRevision *cards,size_t count);
/* Capture immutable selection revisions and complete board ordering in one
 * read transaction. *output starts NULL or owned by this API; success replaces
 * it and publishes the board/peer cache from that same read snapshot, so a
 * destination ordinal describes the captured ordering. Failure preserves all
 * outputs. Caller frees on cancellation. */
int wena_hierarchy_mutation_selected_move_load(void *context,const char *board,
    const WenaId *ids,size_t count,WenaCardMoveSelection **output);
int wena_hierarchy_mutation_selected_move_request(WenaHierarchyMutation *adapter,
    const WenaCardMoveSelection *selection,const char *list,const char *lane,
    size_t before,unsigned long request);
int wena_hierarchy_mutation_selected_move(void *context,const WenaCardMoveSelection *selection,
    const char *list,const char *lane,size_t before);
/* Non-owning transfer context. Source and destination storage must be distinct
 * and outlive the context. Optional traversal counts must also be distinct.
 * The caller authorizes access to both boards before capture/save. */
typedef struct WenaHierarchyTransfer {
    WenaHierarchyMutation *source;
    WenaSqliteBoardSnapshot *destination;
    size_t *published_card_count;
} WenaHierarchyTransfer;
int wena_hierarchy_transfer_init(WenaHierarchyTransfer *transfer,WenaHierarchyMutation *source,
    WenaSqliteBoardSnapshot *destination);
/* Initialize *output to NULL or storage owned by this API. Capture reads both
 * complete boards and guards in one transaction, replacing all outputs only
 * after commit. Failure preserves captures, both views and peer counts. */
int wena_hierarchy_transfer_load(void *context,const char *board,const WenaId *ids,size_t count,
    const char *target,WenaCardTransferSelection **output);
int wena_hierarchy_transfer_request(WenaHierarchyTransfer *transfer,const WenaCardTransferSelection *selection,
    const char *list,const char *lane,size_t before,unsigned long request);
int wena_hierarchy_transfer_save(void *context,const WenaCardTransferSelection *selection,
    const char *list,const char *lane,size_t before);
#endif
