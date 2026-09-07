#include "card_mutation.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static int identifier(const char *text)
{
    size_t n;
    unsigned char c;
    if (!text || !text[0]) return 0;
    for (n = 0; n < WENA_ID_CAPACITY; ++n) {
        c = (unsigned char)text[n];
        if (!c) return 1;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_')) return 0;
    }
    return 0;
}

static int title_valid(const char *title)
{
    size_t i, length;
    unsigned int c, code, need, minimum;
    if (!title) return 0;
    for (length = 0; length < 129 && title[length]; ++length) {}
    if (!length || length >= 129) return 0;
    i = 0;
    while (i < length) {
        c = (unsigned char)title[i++];
        if (c < 32u || c == 127u) return 0;
        if (c < 128u) continue;
        if (c >= 194u && c <= 223u) {
            code = c & 31u; need = 1; minimum = 128u;
        } else if (c >= 224u && c <= 239u) {
            code = c & 15u; need = 2; minimum = 2048u;
        } else if (c >= 240u && c <= 244u) {
            code = c & 7u; need = 3; minimum = 65536u;
        } else return 0;
        while (need != 0u) {
            if (i >= length) return 0;
            c = (unsigned char)title[i++];
            if ((c & 192u) != 128u) return 0;
            code = (code << 6) | (c & 63u);
            --need;
        }
        if (code < minimum || code > 1114111u ||
            (code >= 55296u && code <= 57343u) ||
            (code >= 128u && code <= 159u)) return 0;
    }
    return 1;
}

static int scoped(WenaCardMutation *a, const char *board, const char *card)
{
    return a && a->persistence.database && identifier(board) &&
        identifier(card) && strcmp(a->board_id, board) == 0;
}

int wena_card_mutation_init(WenaCardMutation *a, sqlite3 *database,
    const char *actor, const char *board, WenaCard *cards, size_t count)
{
    if (!a) return 0;
    memset(a, 0, sizeof(*a));
    if (!database || !identifier(actor) || !identifier(board) ||
        (count && !cards)) return 0;
    wena_sqlite_persistence_init(&a->persistence, database);
    strcpy(a->actor_id, actor);
    strcpy(a->board_id, board);
    sprintf(a->route, "/b/%s/native", board);
    a->cards = cards;
    a->card_count = count;
    return 1;
}

int wena_card_mutation_load(void *context, const char *board, const char *card,
    char *title, size_t capacity, unsigned long *version)
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
        "AND archived=0 AND EXISTS(SELECT 1 FROM actors WHERE id=?3)",
        -1, &statement, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(statement, 1, card, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, board, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, a->actor_id, -1, SQLITE_TRANSIENT);
    ok = 0;
    if (sqlite3_step(statement) == SQLITE_ROW) {
        value = sqlite3_column_int64(statement, 1);
        stored = sqlite3_column_text(statement, 0);
        bytes = sqlite3_column_bytes(statement, 0);
        if (stored && bytes > 0 && (size_t)bytes < capacity &&
            (size_t)bytes == strlen((const char *)stored) &&
            title_valid((const char *)stored) && value > 0 &&
            value < LONG_MAX) {
            memcpy(title, stored, (size_t)bytes + 1);
            *version = (unsigned long)value;
            ok = 1;
        }
    }
    sqlite3_finalize(statement);
    return ok;
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
    if (!scoped(a, board, card) || !title_valid(title) || !expected ||
        expected >= (unsigned long)LONG_MAX || !request ||
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
        expected >= (unsigned long)LONG_MAX || !request ||
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
