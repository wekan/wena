#include "card_mutation.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static int scoped(WenaCardMutation *a, const char *board, const char *card)
{
    return a && a->persistence.database && wena_model_identifier_valid(board) &&
        wena_model_identifier_valid(card) && strcmp(a->board_id, board) == 0;
}

int wena_card_mutation_init(WenaCardMutation *a, sqlite3 *database,
    const char *actor, const char *board, WenaCard *cards, size_t count)
{
    if (!a) return 0;
    memset(a, 0, sizeof(*a));
    if (!database || !wena_model_identifier_valid(actor) ||
        !wena_model_identifier_valid(board) ||
        (count && !cards)) return 0;
    wena_sqlite_persistence_init(&a->persistence, database);
    strcpy(a->actor_id, actor);
    strcpy(a->board_id, board);
    sprintf(a->route, "/b/%s/native", board);
    a->cards = cards;
    a->card_count = count;
    return 1;
}

static int load_state(void *context, const char *board, const char *card,
    char *title, size_t capacity, unsigned long *version, int archived)
{
    WenaCardMutation *a;
    sqlite3_stmt *statement;
    sqlite3_int64 value;
    const unsigned char *stored;
    int bytes, ok;
    a = (WenaCardMutation *)context;
    if (!scoped(a, board, card) || !title || !capacity || !version) return 0;
    if (sqlite3_prepare_v2(a->persistence.database,
        "SELECT title,version FROM cards WHERE id=?1 AND board_id=?2 "
        "AND archived=?4 AND EXISTS(SELECT 1 FROM actors WHERE id=?3)",
        -1, &statement, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(statement, 1, card, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, board, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, a->actor_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 4, archived);
    ok = 0;
    if (sqlite3_step(statement) == SQLITE_ROW) {
        value = sqlite3_column_int64(statement, 1);
        stored = sqlite3_column_text(statement, 0);
        bytes = sqlite3_column_bytes(statement, 0);
        if (sqlite3_column_type(statement, 0) == SQLITE_TEXT &&
            sqlite3_column_type(statement, 1) == SQLITE_INTEGER && stored && bytes > 0 && (size_t)bytes < capacity &&
            (size_t)bytes == strlen((const char *)stored) &&
            wena_model_title_string_valid((const char *)stored, 129u) && value > 0 &&
            value <= (sqlite3_int64)WENA_VERSION_READ_MAX) {
            memcpy(title, stored, (size_t)bytes + 1);
            *version = (unsigned long)value;
            ok = 1;
        }
    }
    sqlite3_finalize(statement);
    return ok;
}

int wena_card_mutation_load(void *context, const char *board, const char *card,
    char *title, size_t capacity, unsigned long *version)
{
    return load_state(context, board, card, title, capacity, version, 0);
}

int wena_card_mutation_load_archived(void *context, const char *board,
    const char *card, char *title, size_t capacity, unsigned long *version)
{
    return load_state(context, board, card, title, capacity, version, 1);
}

int wena_card_mutation_save_request(WenaCardMutation *a, const char *board,
    const char *card, unsigned long expected, unsigned long request,
    const char *title)
{
    WenaDomainCommand command;
    WenaRegionResponse response;
    char encoded[385];
    const char hex[] = "0123456789ABCDEF";
    size_t n, out, index;
    if (!scoped(a, board, card) || !wena_model_title_string_valid(title, 129u) || !expected ||
        expected > WENA_VERSION_MUTATE_MAX || !request ||
        request >= (unsigned long)LONG_MAX) return 0;
    out = 0;
    for (n = 0; title[n]; ++n) {
        unsigned char c;
        c = (unsigned char)title[n];
        encoded[out++] = '%';
        encoded[out++] = hex[c >> 4];
        encoded[out++] = hex[c & 15];
    }
    encoded[out] = '\0';
    memset(&command, 0, sizeof(command));
    command.operation = WENA_DOMAIN_EDIT_CARD_TITLE;
    command.request_version = request;
    strcpy(command.user_id, a->actor_id);
    strcpy(command.route, a->route);
    sprintf(command.form_body, "cardId=%s&expectedVersion=%lu&title=%s",
        card, expected, encoded);
    command.form_body_length = strlen(command.form_body);
    if (!wena_sqlite_persistence_apply(&a->persistence, &command, &response))
        return 0;
    /* Never update caller-owned display models before the SQLite commit. */
    for (index = 0; index < a->card_count; ++index) {
        if (!strcmp(a->cards[index].id, card) &&
            !strcmp(a->cards[index].board_id, board))
            strcpy(a->cards[index].title, title);
    }
    return 1;
}

static unsigned long next_request(WenaCardMutation *a, const char *operation)
{
    sqlite3_stmt *statement;
    sqlite3_int64 version;
    if (sqlite3_prepare_v2(a->persistence.database,
        "SELECT COALESCE(max(request_version),0) FROM idempotency_keys "
        "WHERE actor_id=?1 AND route=?2 AND operation=?3",
        -1, &statement, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(statement, 1, a->actor_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, a->route, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, operation, -1, SQLITE_TRANSIENT);
    version = -1;
    if (sqlite3_step(statement) == SQLITE_ROW)
        version = sqlite3_column_int64(statement, 0);
    sqlite3_finalize(statement);
    /* Concurrent clients may choose the same identity; the existing atomic
     * idempotency insert then rejects one with no partial write. */
    if (version < 0 || version >= LONG_MAX - 1) return 0;
    return (unsigned long)version + 1;
}

int wena_card_mutation_save(void *context, const char *board, const char *card,
    unsigned long expected, const char *title)
{
    WenaCardMutation *a;
    a = (WenaCardMutation *)context;
    if (!scoped(a, board, card)) return 0;
    return wena_card_mutation_save_request(a, board, card, expected,
        next_request(a, "edit-card-title"), title);
}

int wena_card_mutation_archive_request(WenaCardMutation *a,
    const char *board, const char *card, unsigned long expected,
    unsigned long request)
{
    WenaDomainCommand command;
    WenaRegionResponse response;
    size_t index;
    if (!scoped(a, board, card) || !expected ||
        expected > WENA_VERSION_MUTATE_MAX || !request ||
        request >= (unsigned long)LONG_MAX) return 0;
    memset(&command, 0, sizeof(command));
    command.operation = WENA_DOMAIN_ARCHIVE_CARD;
    command.request_version = request;
    strcpy(command.user_id, a->actor_id);
    strcpy(command.route, a->route);
    sprintf(command.form_body, "cardId=%s&expectedVersion=%lu", card, expected);
    command.form_body_length = strlen(command.form_body);
    if (!wena_sqlite_persistence_apply(&a->persistence, &command, &response))
        return 0;
    for (index = 0; index < a->card_count; ++index) {
        if (!strcmp(a->cards[index].id, card) &&
            !strcmp(a->cards[index].board_id, board))
            a->cards[index].archived = 1;
    }
    return 1;
}

int wena_card_mutation_archive(void *context, const char *board,
    const char *card, unsigned long expected)
{
    WenaCardMutation *a;
    a = (WenaCardMutation *)context;
    if (!scoped(a, board, card)) return 0;
    return wena_card_mutation_archive_request(a, board, card, expected,
        next_request(a, "archive-card"));
}

int wena_card_mutation_set_create_cache(WenaCardMutation *a,
    size_t *count, size_t capacity)
{
    if (!a || !a->cards || !count || *count != a->card_count ||
        capacity < *count) return 0;
    a->published_card_count = count;
    a->card_capacity = capacity;
    return 1;
}

int wena_card_mutation_create_request(WenaCardMutation *a,
    const char *board, const char *list, const char *lane,
    unsigned long request, const char *title)
{
    WenaDomainCommand command;
    WenaRegionResponse response;
    WenaCard staged;
    char encoded[385];
    const char hex[] = "0123456789ABCDEF";
    size_t n, out;
    if (!scoped(a, board, list) || !wena_model_identifier_valid(lane) ||
        !wena_model_title_string_valid(title, 129u) ||
        !request || request >= (unsigned long)LONG_MAX ||
        !a->published_card_count || !a->cards ||
        *a->published_card_count != a->card_count ||
        a->card_count >= a->card_capacity) return 0;
    /* Preflight all fallible model work and cache space before persistence. */
    if (!wena_card_init(&staged, "pending", board, lane, list, title, 0.0, 0))
        return 0;
    out = 0;
    for (n = 0; title[n]; ++n) {
        unsigned char c;
        c = (unsigned char)title[n];
        encoded[out++] = '%';
        encoded[out++] = hex[c >> 4];
        encoded[out++] = hex[c & 15];
    }
    encoded[out] = '\0';
    memset(&command, 0, sizeof(command));
    command.operation = WENA_DOMAIN_CREATE_CARD;
    command.request_version = request;
    strcpy(command.user_id, a->actor_id);
    strcpy(command.route, a->route);
    sprintf(command.form_body, "targetListId=%s&targetSwimlaneId=%s&title=%s",
        list, lane, encoded);
    command.form_body_length = strlen(command.form_body);
    if (!wena_sqlite_persistence_apply(&a->persistence, &command, &response))
        return 0;
    strcpy(staged.id, a->persistence.created_card_id);
    staged.sort = a->persistence.created_card_position;
    a->cards[a->card_count] = staged;
    ++a->card_count;
    *a->published_card_count = a->card_count;
    return 1;
}

int wena_card_mutation_create(void *context, const char *board,
    const char *list, const char *lane, const char *title)
{
    WenaCardMutation *a;
    a = (WenaCardMutation *)context;
    if (!scoped(a, board, list)) return 0;
    return wena_card_mutation_create_request(a, board, list, lane,
        next_request(a, "create-card"), title);
}

int wena_card_mutation_move_request(WenaCardMutation *a,
    const char *board, const char *card, unsigned long expected,
    unsigned long request, const char *list, const char *lane)
{
    WenaDomainCommand command;
    WenaRegionResponse response;
    WenaCard *selected;
    WenaCard moved;
    size_t index, selected_index;
    if (!scoped(a, board, card) || !wena_model_identifier_valid(list) ||
        !wena_model_identifier_valid(lane) ||
        !expected || expected > WENA_VERSION_MUTATE_MAX ||
        !request || request >= (unsigned long)LONG_MAX ||
        (a->published_card_count && *a->published_card_count != a->card_count))
        return 0;
    selected = NULL;
    selected_index = 0;
    for (index = 0; index < a->card_count; ++index) {
        if (!strcmp(a->cards[index].id, card) &&
            !strcmp(a->cards[index].board_id, board)) {
            if (selected || a->cards[index].archived) return 0;
            selected = &a->cards[index];
            selected_index = index;
        }
    }
    /* A native move must have one publishable current model. Missing or
     * ambiguous caches fail before any persistent mutation. */
    if (!selected) return 0;
    memset(&command, 0, sizeof(command));
    command.operation = WENA_DOMAIN_MOVE_CARD;
    command.request_version = request;
    strcpy(command.user_id, a->actor_id);
    strcpy(command.route, a->route);
    sprintf(command.form_body,
        "cardId=%s&expectedVersion=%lu&targetListId=%s&targetSwimlaneId=%s",
        card, expected, list, lane);
    command.form_body_length = strlen(command.form_body);
    if (!wena_sqlite_persistence_apply(&a->persistence, &command, &response))
        return 0;
    moved = *selected;
    strcpy(moved.list_id, list);
    strcpy(moved.swimlane_id, lane);
    moved.sort = a->persistence.moved_card_position;
    /* Components traverse cache order. SQLite appended this card after every
     * destination card, so publish it last while preserving all other order.
     * No allocation or fallible work follows the commit. */
    if (selected_index + 1 < a->card_count)
        memmove(&a->cards[selected_index], &a->cards[selected_index + 1],
            (a->card_count - selected_index - 1) * sizeof(a->cards[0]));
    a->cards[a->card_count - 1] = moved;
    return 1;
}

int wena_card_mutation_move(void *context, const char *board,
    const char *card, unsigned long expected, const char *list, const char *lane)
{
    WenaCardMutation *a;
    a = (WenaCardMutation *)context;
    if (!scoped(a, board, card)) return 0;
    return wena_card_mutation_move_request(a, board, card, expected,
        next_request(a, "move-card"), list, lane);
}

int wena_card_mutation_restore_request(WenaCardMutation *a,
    const char *board, const char *card, unsigned long expected,
    unsigned long request)
{
    WenaDomainCommand command;
    WenaRegionResponse response;
    WenaCard *selected;
    size_t index;
    if (!scoped(a, board, card) || !expected ||
        expected > WENA_VERSION_MUTATE_MAX || !request ||
        request >= (unsigned long)LONG_MAX ||
        (a->published_card_count && *a->published_card_count != a->card_count))
        return 0;
    selected = NULL;
    for (index = 0; index < a->card_count; ++index) {
        if (!strcmp(a->cards[index].id, card) &&
            !strcmp(a->cards[index].board_id, board)) {
            if (selected || a->cards[index].archived != 1) return 0;
            selected = &a->cards[index];
        }
    }
    if (!selected) return 0;
    memset(&command, 0, sizeof(command));
    command.operation = WENA_DOMAIN_RESTORE_CARD;
    command.request_version = request;
    strcpy(command.user_id, a->actor_id);
    strcpy(command.route, a->route);
    sprintf(command.form_body, "cardId=%s&expectedVersion=%lu", card, expected);
    command.form_body_length = strlen(command.form_body);
    if (!wena_sqlite_persistence_apply(&a->persistence, &command, &response))
        return 0;
    /* Archived cards reserve their existing unique position in SQLite and in
     * the cache. Restore only changes visibility after its commit. */
    selected->archived = 0;
    return 1;
}

int wena_card_mutation_restore(void *context, const char *board,
    const char *card, unsigned long expected)
{
    WenaCardMutation *a;
    a = (WenaCardMutation *)context;
    if (!scoped(a, board, card)) return 0;
    return wena_card_mutation_restore_request(a, board, card, expected,
        next_request(a, "restore-card"));
}

int wena_card_mutation_reorder_request(WenaCardMutation *a,
    const char *board,const char *card,unsigned long expected,
    unsigned long request,unsigned long target)
{
    WenaDomainCommand command;
    WenaRegionResponse response;
    WenaSha256 hash;
    WenaCard moved;
    WenaId list,lane;
    char order[65];
    size_t indices[2048],index,count,source,ordinal;
    unsigned long position,previous;
    int found;
    if(!scoped(a,board,card)||!expected||expected > WENA_VERSION_MUTATE_MAX||
        !request||request>=(unsigned long)LONG_MAX||!a->cards||a->card_count>2048||
        (a->published_card_count&&*a->published_card_count!=a->card_count))return 0;
    source=0;found=0;
    for(index=0;index<a->card_count;++index){
        if(!wena_model_identifier_valid(a->cards[index].id)||
            !wena_model_identifier_valid(a->cards[index].board_id)||
            !wena_model_identifier_valid(a->cards[index].list_id)||
            !wena_model_identifier_valid(a->cards[index].swimlane_id))return 0;
        if(!strcmp(a->cards[index].id,card)&&!strcmp(a->cards[index].board_id,board)){
            if(found||a->cards[index].archived)return 0;
            source=index;found=1;
        }
    }
    if(!found)return 0;
    strcpy(list,a->cards[source].list_id);strcpy(lane,a->cards[source].swimlane_id);
    wena_sha256_init(&hash);count=0;previous=0;ordinal=0;
    for(index=0;index<a->card_count;++index){
        WenaCard *row;
        row=&a->cards[index];
        if(strcmp(row->board_id,board)||strcmp(row->list_id,list)||strcmp(row->swimlane_id,lane))continue;
        if((row->archived!=0&&row->archived!=1)||
            !(row->sort>=0&&row->sort<(double)(LONG_MAX-2048)&&row->sort<=9007199254740991.0))return 0;
        position=(unsigned long)row->sort;
        if((double)position!=row->sort||(count&&position<=previous))return 0;
        if(!wena_sqlite_card_order_add(&hash,row->id,strlen(row->id),position))return 0;
        previous=position;indices[count]=index;if(index==source)ordinal=count;++count;
    }
    if(target>=(unsigned long)count)return 0;
    wena_sha256_final_hex(&hash,order);
    memset(&command,0,sizeof(command));command.operation=WENA_DOMAIN_MOVE_CARD;
    command.request_version=request;strcpy(command.user_id,a->actor_id);strcpy(command.route,a->route);
    sprintf(command.form_body,"cardId=%s&expectedVersion=%lu&targetListId=%s&targetSwimlaneId=%s&targetPosition=%lu&expectedOrder=%s",
        card,expected,list,lane,target,order);command.form_body_length=strlen(command.form_body);
    if(!wena_sqlite_persistence_apply(&a->persistence,&command,&response))return 0;
    if(ordinal==(size_t)target)return 1;
    moved=a->cards[source];
    if(ordinal<(size_t)target){for(index=ordinal;index<(size_t)target;++index)a->cards[indices[index]]=a->cards[indices[index+1]];}
    else{for(index=ordinal;index>(size_t)target;--index)a->cards[indices[index]]=a->cards[indices[index-1]];}
    a->cards[indices[target]]=moved;
    for(index=0;index<count;++index)a->cards[indices[index]].sort=(double)index;
    return 1;
}

int wena_card_mutation_reorder(void *context,const char *board,const char *card,
    unsigned long expected,unsigned long target)
{
    WenaCardMutation *a;
    a=(WenaCardMutation*)context;
    if(!scoped(a,board,card))return 0;
    return wena_card_mutation_reorder_request(a,board,card,expected,
        next_request(a,"move-card"),target);
}
