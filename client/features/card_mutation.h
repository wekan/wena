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
#endif
