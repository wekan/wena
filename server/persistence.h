#ifndef WENA_SERVER_PERSISTENCE_H
#define WENA_SERVER_PERSISTENCE_H

#include "domain_operation.h"

#define WENA_MEMORY_MAX_CARDS 16u
#define WENA_MEMORY_MAX_KEYS 32u

typedef struct WenaPersistenceTransaction {
    int (*begin)(void *, const WenaDomainCommand *);
    int (*apply)(void *, const WenaDomainCommand *, WenaRegionResponse *);
    void (*finish)(void *, int);
    void *context;
} WenaPersistenceTransaction;

typedef struct WenaMemoryCard {
    int active;
    int archived;
    char id[65];
    char title[129];
    unsigned long version;
} WenaMemoryCard;

typedef struct WenaMemoryReplayKey {
    char user_id[65];
    char route[257];
    WenaDomainOperation operation;
    unsigned long request_version;
} WenaMemoryReplayKey;

typedef struct WenaMemoryStore {
    char authorized_user[65];
    WenaMemoryCard cards[WENA_MEMORY_MAX_CARDS];
    size_t card_count;
    WenaMemoryReplayKey keys[WENA_MEMORY_MAX_KEYS];
    size_t key_count;
    WenaMemoryReplayKey pending_key;
    struct WenaMemoryStore *staged;
    int transaction_active;
} WenaMemoryStore;

int wena_persistence_execute(const WenaPersistenceTransaction *transaction,
                             const WenaDomainCommand *command,
                             WenaRegionResponse *response);
void wena_memory_store_init(WenaMemoryStore *store, const char *authorized_user);
void wena_memory_transaction(WenaMemoryStore *store,
                             WenaPersistenceTransaction *transaction);

#endif
