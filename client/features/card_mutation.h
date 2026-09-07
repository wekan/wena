#ifndef WENA_CARD_MUTATION_H
#define WENA_CARD_MUTATION_H

#include "../../models/card.h"
#include "../../server/sqlite_persistence.h"

/* The caller supplies an authenticated local actor and its authorized board.
 * This adapter never creates actors or grants board access. SQLite must outlive
 * the adapter; recreate it after closing/reopening the database. */
typedef struct WenaCardMutation {
    WenaSqlitePersistence persistence;
    WenaId actor_id;
    WenaId board_id;
    char route[257];
    WenaCard *cards;
    size_t card_count;
    size_t *published_card_count;
    size_t card_capacity;
} WenaCardMutation;

int wena_card_mutation_init(WenaCardMutation *adapter, sqlite3 *database,
    const char *authenticated_actor, const char *authorized_board,
    WenaCard *cards, size_t card_count);
int wena_card_mutation_load(void *context, const char *board_id,
    const char *card_id, char *title, size_t capacity, unsigned long *version);
int wena_card_mutation_save(void *context, const char *board_id,
    const char *card_id, unsigned long expected_version, const char *title);
/* Explicit request identity supports deterministic retries: a committed replay
 * is rejected and never mutates SQLite or the caller-owned model again. */
int wena_card_mutation_save_request(WenaCardMutation *adapter,
    const char *board_id, const char *card_id, unsigned long expected_version,
    unsigned long request_version, const char *title);
int wena_card_mutation_archive(void *context, const char *board_id,
    const char *card_id, unsigned long expected_version);
int wena_card_mutation_archive_request(WenaCardMutation *adapter,
    const char *board_id, const char *card_id, unsigned long expected_version,
    unsigned long request_version);
/* Creation is disabled until a caller-owned count and bounded array capacity
 * are explicitly registered. The backing array is the one passed to init. */
int wena_card_mutation_set_create_cache(WenaCardMutation *adapter,
    size_t *card_count, size_t capacity);
int wena_card_mutation_create(void *context, const char *board_id,
    const char *list_id, const char *swimlane_id, const char *title);
int wena_card_mutation_create_request(WenaCardMutation *adapter,
    const char *board_id, const char *list_id, const char *swimlane_id,
    unsigned long request_version, const char *title);
int wena_card_mutation_move(void *context, const char *board_id,
    const char *card_id, unsigned long expected_version,
    const char *target_list_id, const char *target_swimlane_id);
int wena_card_mutation_move_request(WenaCardMutation *adapter,
    const char *board_id, const char *card_id, unsigned long expected_version,
    unsigned long request_version, const char *target_list_id,
    const char *target_swimlane_id);
int wena_card_mutation_load_archived(void *context, const char *board_id,
    const char *card_id, char *title, size_t capacity, unsigned long *version);
int wena_card_mutation_restore(void *context, const char *board_id,
    const char *card_id, unsigned long expected_version);
int wena_card_mutation_restore_request(WenaCardMutation *adapter,
    const char *board_id, const char *card_id, unsigned long expected_version,
    unsigned long request_version);
/* target_position is an ordinal over the complete current column, including
 * archived cards. A real reorder compacts that column's gaps in the same
 * transaction. Same-ordinal requests validate scope/version/order but preserve
 * positions, versions and idempotency metadata. Other columns stay unchanged.
 * Requires at most 2048 cached cards and exact nonnegative integer positions
 * below LONG_MAX-2048 and at most 2^53-1; malformed/overflow data fails closed. */
int wena_card_mutation_reorder(void *context, const char *board_id,
    const char *card_id, unsigned long expected_version,
    unsigned long target_position);
int wena_card_mutation_reorder_request(WenaCardMutation *adapter,
    const char *board_id, const char *card_id, unsigned long expected_version,
    unsigned long request_version, unsigned long target_position);
#endif
