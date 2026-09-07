#ifndef WENA_CARD_DESCRIPTION_MUTATION_H
#define WENA_CARD_DESCRIPTION_MUTATION_H
#include "../../models/model.h"
#include "../../server/sqlite_persistence.h"

typedef struct WenaCardDescriptionMutation {
    WenaSqlitePersistence persistence;
    WenaId actor_id;
    WenaId board_id;
    char route[257];
} WenaCardDescriptionMutation;

/* Caller supplies an authenticated actor and authorized board. Requires the
 * additive card_descriptions schema: absence fails closed without card writes.
 * Absent rows read as empty. Editors own descriptions; cards.version supplies
 * optimistic concurrency shared with every other card mutation. */
int wena_card_description_mutation_init(WenaCardDescriptionMutation *adapter,
    sqlite3 *database,const char *actor_id,const char *board_id);
int wena_card_description_mutation_load(void *context,const char *board_id,
    const char *card_id,char *description,size_t capacity,unsigned long *version);
int wena_card_description_mutation_save(void *context,const char *board_id,
    const char *card_id,unsigned long expected_version,const char *description);
int wena_card_description_mutation_save_request(WenaCardDescriptionMutation *adapter,
    const char *board_id,const char *card_id,unsigned long expected_version,
    unsigned long request_version,const char *description);
#endif
