#include "persistence.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int form_value(const WenaDomainCommand *command, const char *name,
                      char *output, size_t capacity)
{
    size_t cursor;
    int found;
    cursor = 0u; found = 0;
    while (cursor < command->form_body_length) {
        size_t end;
        size_t equal;
        end = cursor;
        while (end < command->form_body_length && command->form_body[end] != '&') ++end;
        equal = cursor;
        while (equal < end && command->form_body[equal] != '=') ++equal;
        if (equal == end) return 0;
        if (equal - cursor == strlen(name) &&
            memcmp(command->form_body + cursor, name, equal - cursor) == 0) {
            size_t length;
            if (found) return 0;
            length = end - equal - 1u;
            if (length == 0u || length >= capacity) return 0;
            memcpy(output, command->form_body + equal + 1u, length);
            output[length] = '\0'; found = 1;
        }
        cursor = end + 1u;
    }
    return found;
}

static int positive_number(const char *value, unsigned long *output)
{
    unsigned long result;
    const unsigned char *cursor;
    if (value == NULL || value[0] == '\0') return 0;
    result = 0ul;
    for (cursor = (const unsigned char *)value; *cursor != '\0'; ++cursor) {
        unsigned long digit;
        if (*cursor < '0' || *cursor > '9') return 0;
        digit = (unsigned long)(*cursor - '0');
        if (result > (~0ul - digit) / 10ul) return 0;
        result = result * 10ul + digit;
    }
    if (result == 0ul) return 0;
    *output = result;
    return 1;
}

int wena_persistence_execute(const WenaPersistenceTransaction *transaction,
                             const WenaDomainCommand *command,
                             WenaRegionResponse *response)
{
    WenaRegionResponse candidate;
    char wire[WENA_REGION_RESPONSE_MAX_BYTES];
    size_t wire_length;
    if (response != NULL) memset(response, 0, sizeof(*response));
    if (transaction == NULL || transaction->begin == NULL || transaction->apply == NULL ||
        transaction->finish == NULL || command == NULL || response == NULL ||
        command->user_id[0] == '\0' || strncmp(command->route, "/b/", 3) != 0 ||
        command->request_version == 0ul || command->form_body_length == 0u ||
        command->form_body_length >= WENA_DOMAIN_BODY_CAPACITY) return 0;
    if (!transaction->begin(transaction->context, command)) return 0;
    memset(&candidate, 0, sizeof(candidate));
    candidate.request_version = command->request_version;
    if (!transaction->apply(transaction->context, command, &candidate) ||
        !wena_region_response_encode(&candidate, wire, sizeof(wire), &wire_length)) {
        transaction->finish(transaction->context, 0);
        return 0;
    }
    transaction->finish(transaction->context, 1);
    *response = candidate;
    return 1;
}

void wena_memory_store_init(WenaMemoryStore *store, const char *authorized_user)
{
    if (store == NULL) return;
    memset(store, 0, sizeof(*store));
    if (authorized_user != NULL && strlen(authorized_user) < sizeof(store->authorized_user))
        strcpy(store->authorized_user, authorized_user);
}

static int memory_begin(void *context, const WenaDomainCommand *command)
{
    WenaMemoryStore *store;
    size_t index;
    store = (WenaMemoryStore *)context;
    if (store == NULL || store->transaction_active || store->authorized_user[0] == '\0' ||
        strcmp(store->authorized_user, command->user_id) != 0 ||
        store->key_count >= WENA_MEMORY_MAX_KEYS) return 0;
    for (index = 0; index < store->key_count; ++index)
        if (store->keys[index].request_version == command->request_version &&
            store->keys[index].operation == command->operation &&
            strcmp(store->keys[index].user_id, command->user_id) == 0 &&
            strcmp(store->keys[index].route, command->route) == 0) return 0;
    store->staged = (WenaMemoryStore *)malloc(sizeof(*store));
    if (store->staged == NULL) return 0;
    *store->staged = *store;
    store->staged->staged = NULL;
    store->staged->transaction_active = 0;
    memset(&store->pending_key, 0, sizeof(store->pending_key));
    strcpy(store->pending_key.user_id, command->user_id);
    strcpy(store->pending_key.route, command->route);
    store->pending_key.operation = command->operation;
    store->pending_key.request_version = command->request_version;
    store->transaction_active = 1;
    return 1;
}

static WenaMemoryCard *find_card(WenaMemoryStore *store, const char *id)
{
    size_t index;
    for (index = 0; index < store->card_count; ++index)
        if (store->cards[index].active && strcmp(store->cards[index].id, id) == 0)
            return &store->cards[index];
    return NULL;
}

static int memory_apply(void *context, const WenaDomainCommand *command,
                        WenaRegionResponse *response)
{
    WenaMemoryStore *store;
    WenaMemoryStore *staged;
    WenaMemoryCard *card;
    char card_id[65];
    char title[129];
    char expected_text[32];
    unsigned long expected;
    store = (WenaMemoryStore *)context;
    if (store == NULL || !store->transaction_active || store->staged == NULL) return 0;
    staged = store->staged; card = NULL;
    if (command->operation == WENA_DOMAIN_CREATE_CARD) {
        if (!form_value(command, "title", title, sizeof(title)) ||
            staged->card_count >= WENA_MEMORY_MAX_CARDS) return 0;
        card = &staged->cards[staged->card_count++];
        memset(card, 0, sizeof(*card)); card->active = 1;
        sprintf(card->id, "card-%lu", command->request_version);
        strcpy(card->title, title); card->version = 1ul;
    } else {
        if (!form_value(command, "cardId", card_id, sizeof(card_id)) ||
            !form_value(command, "expectedVersion", expected_text, sizeof(expected_text)) ||
            !positive_number(expected_text, &expected) ||
            (card = find_card(staged, card_id)) == NULL || card->version != expected)
            return 0;
        if (command->operation == WENA_DOMAIN_EDIT_CARD_TITLE) {
            if (!form_value(command, "title", title, sizeof(title)) || card->archived) return 0;
            strcpy(card->title, title);
        } else if (command->operation == WENA_DOMAIN_ARCHIVE_CARD) {
            if (card->archived) return 0;
            card->archived = 1;
        } else return 0;
        ++card->version;
    }
    response->region_count = 1u;
    strcpy(response->regions[0].name, "board");
    response->regions[0].version = command->request_version;
    strcpy(response->regions[0].content, card->title);
    response->regions[0].content_length = strlen(card->title);
    return 1;
}

static void memory_finish(void *context, int commit)
{
    WenaMemoryStore *store;
    WenaMemoryStore *staged;
    WenaMemoryReplayKey pending;
    store = (WenaMemoryStore *)context;
    if (store == NULL || !store->transaction_active || store->staged == NULL) return;
    staged = store->staged; pending = store->pending_key;
    if (commit) {
        *store = *staged;
        store->keys[store->key_count++] = pending;
    }
    free(staged);
    store->staged = NULL; store->transaction_active = 0;
}

void wena_memory_transaction(WenaMemoryStore *store,
                             WenaPersistenceTransaction *transaction)
{
    if (transaction == NULL) return;
    transaction->begin = memory_begin; transaction->apply = memory_apply;
    transaction->finish = memory_finish; transaction->context = store;
}
