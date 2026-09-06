#include "../server/persistence.h"

#include <assert.h>
#include <string.h>

static void command(WenaDomainCommand *item, WenaDomainOperation operation,
                    unsigned long version, const char *user, const char *body)
{
    memset(item, 0, sizeof(*item));
    item->operation = operation; item->request_version = version;
    strcpy(item->user_id, user); strcpy(item->route, "/b/board-1/demo");
    strcpy(item->form_body, body); item->form_body_length = strlen(body);
}

typedef struct Broken { int began; int applied; int rolled_back; } Broken;
static int broken_begin(void *context, const WenaDomainCommand *unused)
{ Broken *b = (Broken *)context; (void)unused; ++b->began; return 1; }
static int broken_apply(void *context, const WenaDomainCommand *unused,
                        WenaRegionResponse *response)
{ Broken *b = (Broken *)context; (void)unused; ++b->applied; response->region_count = 1u;
  strcpy(response->regions[0].name, "not-allowed"); return 1; }
static void broken_finish(void *context, int commit)
{ Broken *b = (Broken *)context; if (!commit) ++b->rolled_back; }

int main(void)
{
    WenaMemoryStore store;
    WenaPersistenceTransaction transaction;
    WenaDomainCommand item;
    WenaRegionResponse response;
    Broken broken;
    size_t cards;
    wena_memory_store_init(&store, "user-1");
    wena_memory_transaction(&store, &transaction);

    command(&item, WENA_DOMAIN_CREATE_CARD, 1ul, "user-1", "title=First");
    assert(wena_persistence_execute(&transaction, &item, &response));
    assert(store.card_count == 1u && store.key_count == 1u);
    assert(strcmp(store.cards[0].id, "card-1") == 0 && store.cards[0].version == 1ul);
    assert(response.region_count == 1u && strcmp(response.regions[0].content, "First") == 0);
    assert(!wena_persistence_execute(&transaction, &item, &response));
    assert(store.card_count == 1u && store.key_count == 1u && response.region_count == 0u);

    command(&item, WENA_DOMAIN_EDIT_CARD_TITLE, 2ul, "user-1",
            "cardId=card-1&expectedVersion=9&title=Wrong");
    assert(!wena_persistence_execute(&transaction, &item, &response));
    assert(strcmp(store.cards[0].title, "First") == 0 && store.cards[0].version == 1ul);
    command(&item, WENA_DOMAIN_EDIT_CARD_TITLE, 2ul, "user-1",
            "cardId=card-1&expectedVersion=1&title=Second");
    assert(wena_persistence_execute(&transaction, &item, &response));
    assert(strcmp(store.cards[0].title, "Second") == 0 && store.cards[0].version == 2ul);

    command(&item, WENA_DOMAIN_ARCHIVE_CARD, 3ul, "user-1",
            "cardId=card-1&expectedVersion=2");
    assert(wena_persistence_execute(&transaction, &item, &response));
    assert(store.cards[0].archived && store.cards[0].version == 3ul);
    cards = store.card_count;
    command(&item, WENA_DOMAIN_CREATE_CARD, 4ul, "other-user", "title=Forbidden");
    assert(!wena_persistence_execute(&transaction, &item, &response));
    assert(store.card_count == cards && store.key_count == 3u);
    command(&item, WENA_DOMAIN_EDIT_CARD_TITLE, 4ul, "user-1",
            "cardId=card-1&expectedVersion=3&title=AfterArchive");
    assert(!wena_persistence_execute(&transaction, &item, &response));
    assert(strcmp(store.cards[0].title, "Second") == 0 && store.key_count == 3u);

    memset(&broken, 0, sizeof(broken));
    transaction.begin = broken_begin; transaction.apply = broken_apply;
    transaction.finish = broken_finish; transaction.context = &broken;
    command(&item, WENA_DOMAIN_CREATE_CARD, 5ul, "user-1", "title=InvalidResponse");
    assert(!wena_persistence_execute(&transaction, &item, &response));
    assert(broken.began == 1 && broken.applied == 1 && broken.rolled_back == 1);
    assert(response.region_count == 0u);
    return 0;
}
