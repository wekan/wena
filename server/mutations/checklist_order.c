#include "checklist_order.h"
#include "common.h"
#include "../../models/checklist_item.h"
#include <stdlib.h>
#include <string.h>

typedef struct ChecklistOrderRow {
    WenaId id;
    unsigned long position;
    unsigned long version;
} ChecklistOrderRow;

static const char *read_text(sqlite3_stmt *statement, int column, size_t capacity)
{
    const char *text;
    int bytes;
    if (sqlite3_column_type(statement, column) != SQLITE_TEXT) return NULL;
    text = (const char *)sqlite3_column_text(statement, column);
    bytes = sqlite3_column_bytes(statement, column);
    if (!text || bytes < 1 || (size_t)bytes >= capacity ||
        memchr(text, 0, (size_t)bytes)) return NULL;
    return text;
}

static const char *read_id(sqlite3_stmt *statement, int column)
{
    const char *text;
    text = read_text(statement, column, WENA_ID_CAPACITY);
    return text && wena_model_identifier_valid(text) ? text : NULL;
}

static int integer(sqlite3_stmt *statement, int column, unsigned long maximum,
    unsigned long *value)
{
    sqlite3_int64 number;
    if (sqlite3_column_type(statement, column) != SQLITE_INTEGER) return 0;
    number = sqlite3_column_int64(statement, column);
    if (number < 0 || number > (sqlite3_int64)maximum) return 0;
    *value = (unsigned long)number;
    return 1;
}

static int timestamps_valid(sqlite3_stmt *statement, int first, int second)
{
    sqlite3_int64 created, updated;
    if (sqlite3_column_type(statement, first) != SQLITE_INTEGER ||
        sqlite3_column_type(statement, second) != SQLITE_INTEGER) return 0;
    created = sqlite3_column_int64(statement, first);
    updated = sqlite3_column_int64(statement, second);
    return created >= 0 && updated >= created;
}

static int checklist_fields_valid(sqlite3_stmt *statement, const char *id,
    const char *board, const char *card, unsigned long position)
{
    WenaChecklist model;
    unsigned long flag;
    if (!wena_checklist_init(&model, id, board, card,
        read_text(statement, 5, WENA_CHECKLIST_TITLE_CAPACITY), position) ||
        !integer(statement, 6, 1, &flag)) return 0;
    model.hide_checked_items = (int)flag;
    if (!integer(statement, 7, 1, &flag)) return 0;
    model.hide_all_items = (int)flag;
    if (sqlite3_column_type(statement, 8) != SQLITE_NULL) {
        if (!integer(statement, 8, 1, &flag)) return 0;
        model.show_on_minicard = (WenaChecklistMinicard)flag;
    }
    return timestamps_valid(statement, 9, 10) && wena_checklist_valid(&model);
}

static int item_fields_valid(sqlite3_stmt *statement, const char *id,
    const char *board, const char *card, const char *parent, unsigned long position)
{
    WenaChecklistItem model;
    unsigned long finished;
    return integer(statement, 7, 1, &finished) &&
        wena_checklist_item_init(&model, id, board, card, parent,
            read_text(statement, 6, WENA_CHECKLIST_TITLE_CAPACITY), position,
            (int)finished) && timestamps_valid(statement, 8, 9);
}

static int prepare(sqlite3 *db, const char *query, const char *board,
    const char *card, const char *checklist, sqlite3_stmt **statement)
{
    if (sqlite3_prepare_v2(db, query, -1, statement, NULL) != SQLITE_OK) return 0;
    if (sqlite3_bind_text(*statement, 1, board, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(*statement, 2, card, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(*statement, 3, checklist, -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        sqlite3_finalize(*statement); *statement = NULL; return 0;
    }
    return 1;
}

static int read_card_order(sqlite3 *db, const char *board, const char *card,
    const char *checklist, int items, ChecklistOrderRow *rows, size_t *count,
    unsigned long *checklist_version, size_t *item_total)
{
    ChecklistOrderRow lists[WENA_CHECKLIST_MAX_PER_CARD];
    sqlite3_stmt *statement;
    const char *id, *stored_board, *stored_card, *parent;
    size_t list_count, total, index, previous;
    unsigned long position, version, previous_position;
    WenaId previous_parent;
    int step, valid;
    *count = 0; *checklist_version = 0;
    list_count = 0; valid = 0; statement = NULL;
    if (!prepare(db, "SELECT id,board_id,card_id,position,version,title,hide_checked_items,"
        "hide_all_items,show_on_minicard,created_at,updated_at FROM checklists "
        "WHERE card_id=?2 OR id=?3 ORDER BY position,id", board, card, checklist,
        &statement)) return 0;
    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        id = read_id(statement, 0); stored_board = read_id(statement, 1);
        stored_card = read_id(statement, 2);
        if (list_count >= WENA_CHECKLIST_MAX_PER_CARD || !id || !stored_board ||
            !stored_card || strcmp(stored_board, board) || strcmp(stored_card, card) ||
            !integer(statement, 3, WENA_CHECKLIST_POSITION_MAX, &position) ||
            !integer(statement, 4, WENA_VERSION_READ_MAX, &version) || !version ||
            !checklist_fields_valid(statement, id, board, card, position) ||
            (list_count && position <= lists[list_count - 1].position)) goto done;
        for (previous = 0; previous < list_count; ++previous)
            if (!strcmp(id, lists[previous].id)) goto done;
        strcpy(lists[list_count].id, id);
        lists[list_count].position = position; lists[list_count].version = version;
        ++list_count;
        if (!strcmp(id, checklist)) *checklist_version = version;
    }
    if (step != SQLITE_DONE || (*checklist && !*checklist_version)) goto done;
    if (sqlite3_finalize(statement) != SQLITE_OK) return 0;
    statement = NULL;
    /* Stream the complete card item collection, including scope-corrupt
     * selected-checklist children and hidden/finished items. Only this
     * checklist's rows need storage for item ordering. */
    if (!prepare(db, "SELECT id,board_id,card_id,checklist_id,position,version,title,"
        "is_finished,created_at,updated_at FROM "
        "checklist_items WHERE card_id=?2 OR checklist_id=?3 OR checklist_id IN "
        "(SELECT id FROM checklists WHERE card_id=?2) ORDER BY "
        "checklist_id,position,id", board, card, checklist, &statement)) return 0;
    total = 0; previous_parent[0] = 0; previous_position = 0;
    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        id = read_id(statement, 0); stored_board = read_id(statement, 1);
        stored_card = read_id(statement, 2); parent = read_id(statement, 3);
        if (++total > WENA_CHECKLIST_MAX_ITEMS || !id || !stored_board || !stored_card ||
            !parent || strcmp(stored_board, board) || strcmp(stored_card, card) ||
            !integer(statement, 4, WENA_CHECKLIST_POSITION_MAX, &position) ||
            !integer(statement, 5, WENA_VERSION_READ_MAX, &version) || !version ||
            !item_fields_valid(statement, id, board, card, parent, position) ||
            (!strcmp(previous_parent, parent) && position <= previous_position)) goto done;
        for (index = 0; index < list_count; ++index)
            if (!strcmp(lists[index].id, parent)) break;
        if (index == list_count) goto done;
        if (items && !strcmp(parent, checklist)) {
            for (previous = 0; previous < *count; ++previous)
                if (!strcmp(id, rows[previous].id)) goto done;
            strcpy(rows[*count].id, id);
            rows[*count].position = position; rows[*count].version = version;
            ++*count;
        }
        strcpy(previous_parent, parent); previous_position = position;
    }
    if (step != SQLITE_DONE) goto done;
    if (!items) {
        memcpy(rows, lists, list_count * sizeof(*rows)); *count = list_count;
    }
    if (item_total) *item_total = total;
    valid = 1;
done:
    if (statement && sqlite3_finalize(statement) != SQLITE_OK) valid = 0;
    return valid;
}

static int write_order(sqlite3 *db, const char *board, const char *card,
    const char *checklist, int items, ChecklistOrderRow *rows, size_t count,
    size_t source, size_t target)
{
    ChecklistOrderRow moved;
    sqlite3_stmt *statement;
    unsigned long stage;
    size_t index;
    int phase, changed, valid;
    const char *query;
    /* Find a free contiguous block using the original sorted positions.
     * Unlike max+count staging this also works at the maximum stored position.
     * At most1024 occupied positions leave ample room in the integer domain. */
    stage = 0;
    for (index = 0; index < count; ++index) {
        if (rows[index].position < stage) continue;
        if (rows[index].position - stage >= (unsigned long)count) break;
        stage = rows[index].position + 1UL;
    }
    if (stage > WENA_CHECKLIST_POSITION_MAX - (unsigned long)count + 1UL) return 0;
    moved = rows[source];
    if (source < target)
        memmove(rows + source, rows + source + 1, (target - source) * sizeof(*rows));
    else memmove(rows + target + 1, rows + target, (source - target) * sizeof(*rows));
    rows[target] = moved;
    /* All changing row versions must remain readable before any staging write. */
    for (index = 0; index < count; ++index)
        if (rows[index].position != (unsigned long)index &&
            rows[index].version > WENA_VERSION_MUTATE_MAX) return 0;
    for (phase = 0; phase < 2; ++phase) {
        if (items) query = phase ?
            "UPDATE checklist_items SET position=?5,version=version+?7,updated_at="
            "CASE WHEN ?7 THEN max(updated_at,CAST(strftime('%s','now') AS INTEGER)*1000) "
            "ELSE updated_at END WHERE board_id=?1 AND card_id=?2 AND checklist_id=?3 "
            "AND id=?4 AND version=?6" :
            "UPDATE checklist_items SET position=?5 WHERE board_id=?1 AND card_id=?2 "
            "AND checklist_id=?3 AND id=?4 AND version=?6";
        else query = phase ?
            "UPDATE checklists SET position=?5,version=version+?7,updated_at="
            "CASE WHEN ?7 THEN max(updated_at,CAST(strftime('%s','now') AS INTEGER)*1000) "
            "ELSE updated_at END WHERE board_id=?1 AND card_id=?2 AND id=?4 AND version=?6" :
            "UPDATE checklists SET position=?5 WHERE board_id=?1 AND card_id=?2 "
            "AND id=?4 AND version=?6";
        if (!prepare(db, query, board, card, checklist, &statement)) return 0;
        valid = 1;
        /* In the final pass position i is never above stage+i, so ascending
         * writes also work when the free block overlaps final ordinals. */
        for (index = 0; index < count && valid; ++index) {
            changed = rows[index].position != (unsigned long)index;
            valid = sqlite3_bind_text(statement, 4, rows[index].id, -1,
                SQLITE_TRANSIENT) == SQLITE_OK &&
                sqlite3_bind_int64(statement, 5, (sqlite3_int64)(phase ?
                    (unsigned long)index : stage + (unsigned long)index)) == SQLITE_OK &&
                sqlite3_bind_int64(statement, 6, (sqlite3_int64)rows[index].version) == SQLITE_OK &&
                (!phase || sqlite3_bind_int(statement, 7, changed) == SQLITE_OK) &&
                sqlite3_step(statement) == SQLITE_DONE && sqlite3_changes(db) == 1 &&
                sqlite3_reset(statement) == SQLITE_OK;
        }
        if (sqlite3_finalize(statement) != SQLITE_OK) valid = 0;
        if (!valid) return 0;
    }
    return 1;
}

int wena_sqlite_checklist_order(sqlite3 *db, const WenaDomainCommand *command,
    const char *board, unsigned long *result_version)
{
    char card[65], checklist[65], item[65], number[32];
    unsigned long card_version, checklist_version, item_version, stored_version;
    unsigned long target;
    ChecklistOrderRow *rows;
    sqlite3_stmt *statement;
    size_t count, selected;
    int items, valid, result;
    if (!db || !command || !result_version || sqlite3_get_autocommit(db) ||
        !wena_model_identifier_valid(board)) return 0;
    items = command->operation == WENA_DOMAIN_REORDER_CHECKLIST_ITEM;
    if (!items && command->operation != WENA_DOMAIN_REORDER_CHECKLIST) return 0;
    if (!wena_mutation_text(command, "cardId", card, sizeof(card), 0) ||
        !wena_model_identifier_valid(card) ||
        !wena_mutation_text(command, "checklistId", checklist, sizeof(checklist), 0) ||
        !wena_model_identifier_valid(checklist) ||
        !wena_mutation_text(command, "expectedVersion", number, sizeof(number), 0) ||
        !wena_mutation_decimal(number, 0, WENA_VERSION_MUTATE_MAX, &card_version) ||
        !wena_mutation_text(command, "expectedChecklistVersion", number, sizeof(number), 0) ||
        !wena_mutation_decimal(number, 0, WENA_VERSION_MUTATE_MAX, &checklist_version) ||
        !wena_mutation_text(command, "targetPosition", number, sizeof(number), 0) ||
        !wena_mutation_decimal(number, 1, WENA_CHECKLIST_MAX_ITEMS - 1u, &target)) return 0;
    item_version = 0; item[0] = 0;
    if (items && (!wena_mutation_text(command, "itemId", item, sizeof(item), 0) ||
        !wena_model_identifier_valid(item) ||
        !wena_mutation_text(command, "expectedItemVersion", number, sizeof(number), 0) ||
        !wena_mutation_decimal(number, 0, WENA_VERSION_MUTATE_MAX, &item_version))) return 0;
    if (!prepare(db, "SELECT c.version FROM cards c JOIN checklists k ON "
        "k.board_id=c.board_id AND k.card_id=c.id WHERE c.board_id=?1 AND "
        "c.id=?2 AND k.id=?3 AND c.archived=0", board, card, checklist, &statement)) return 0;
    valid = sqlite3_step(statement) == SQLITE_ROW &&
        integer(statement, 0, WENA_VERSION_READ_MAX, &stored_version) &&
        stored_version == card_version;
    if (sqlite3_finalize(statement) != SQLITE_OK) valid = 0;
    if (!valid) return 0;
    rows = (ChecklistOrderRow *)calloc(items ? WENA_CHECKLIST_MAX_ITEMS :
        WENA_CHECKLIST_MAX_PER_CARD, sizeof(*rows));
    if (!rows) return 0;
    result = 0;
    if (!read_card_order(db, board, card, checklist, items, rows, &count,
        &stored_version, NULL) || stored_version != checklist_version || target >= count)
        goto done;
    for (selected = 0; selected < count; ++selected)
        if (!strcmp(rows[selected].id, items ? item : checklist)) break;
    if (selected == count || (items && rows[selected].version != item_version)) goto done;
    if (selected == (size_t)target) { *result_version = card_version; result = 2; goto done; }
    if (!write_order(db, board, card, checklist, items, rows, count, selected,
        (size_t)target)) goto done;
    if (items) {
        if (!prepare(db, "UPDATE checklists SET version=version+1,updated_at="
            "max(updated_at,CAST(strftime('%s','now') AS INTEGER)*1000) WHERE "
            "board_id=?1 AND card_id=?2 AND id=?3 AND version=?4", board, card,
            checklist, &statement)) goto done;
        valid = sqlite3_bind_int64(statement, 4, (sqlite3_int64)checklist_version) == SQLITE_OK &&
            sqlite3_step(statement) == SQLITE_DONE && sqlite3_changes(db) == 1;
        if (sqlite3_finalize(statement) != SQLITE_OK) valid = 0;
        if (!valid) goto done;
    }
    if (!prepare(db, "UPDATE cards SET version=version+1 WHERE board_id=?1 "
        "AND id=?2 AND version=?3 AND archived=0", board, card, checklist, &statement)) goto done;
    valid = sqlite3_bind_int64(statement, 3, (sqlite3_int64)card_version) == SQLITE_OK &&
        sqlite3_step(statement) == SQLITE_DONE && sqlite3_changes(db) == 1;
    if (sqlite3_finalize(statement) != SQLITE_OK) valid = 0;
    if (valid) { *result_version = card_version + 1UL; result = 1; }
done:
    free(rows);
    return result;
}

static int card_revision(sqlite3 *db, const char *board, const char *card,
    unsigned long expected)
{
    sqlite3_stmt *statement;
    unsigned long version;
    int valid;
    if (!prepare(db, "SELECT version FROM cards WHERE board_id=?1 AND id=?2 "
        "AND archived=0 AND ?3='' AND EXISTS(SELECT 1 FROM boards WHERE id=?1)",
        board, card, "", &statement)) return 0;
    valid = sqlite3_step(statement) == SQLITE_ROW &&
        integer(statement, 0, WENA_VERSION_READ_MAX, &version) && version == expected &&
        sqlite3_step(statement) == SQLITE_DONE;
    if (sqlite3_finalize(statement) != SQLITE_OK) valid = 0;
    return valid;
}

/* Missing destination board preserves the original same-board contract. An
 * explicitly supplied board must be a single, nonempty validated identifier. */
static int destination_board(const WenaDomainCommand *command,
    const char *source, char *target)
{
    if (!wena_mutation_has_value(command, "targetBoardId")) {
        strcpy(target, source);
        return 1;
    }
    return wena_mutation_text(command, "targetBoardId", target, WENA_ID_CAPACITY, 0) &&
        wena_model_identifier_valid(target);
}

/* Both transfer operations share guarded aggregate advancement. The caller's
 * transaction rolls back the first update if the second is rejected. */
static int advance_card(sqlite3 *db, const char *board, const char *card,
    unsigned long version)
{
    sqlite3_stmt *statement;
    int valid;
    if (!prepare(db, "UPDATE cards SET version=version+1 WHERE board_id=?1 AND "
        "id=?2 AND ?3='' AND version=?4 AND archived=0", board, card, "",
        &statement)) return 0;
    valid = sqlite3_bind_int64(statement, 4, (sqlite3_int64)version) == SQLITE_OK &&
        sqlite3_step(statement) == SQLITE_DONE && sqlite3_changes(db) == 1;
    if (sqlite3_finalize(statement) != SQLITE_OK) valid = 0;
    return valid && card_revision(db, board, card, version + 1UL);
}

static int advance_transfer_cards(sqlite3 *db, const char *board, const char *card,
    unsigned long version, const char *target_board, const char *target,
    unsigned long target_version)
{
    if (!advance_card(db, board, card, version)) return 0;
    if (!strcmp(card, target)) return !strcmp(board, target_board) && version == target_version;
    return advance_card(db, target_board, target, target_version);
}

static int move_row(sqlite3 *db, const char *query, const char *board,
    const char *card, const char *checklist, const char *target, const char *id,
    unsigned long version, unsigned long position, const char *target_board)
{
    sqlite3_stmt *statement;
    int valid;
    if (!prepare(db, query, board, card, checklist, &statement)) return 0;
    valid = sqlite3_bind_text(statement, 4, target, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
        sqlite3_bind_text(statement, 5, id, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
        sqlite3_bind_int64(statement, 6, (sqlite3_int64)version) == SQLITE_OK &&
        sqlite3_bind_int64(statement, 7, (sqlite3_int64)position) == SQLITE_OK &&
        sqlite3_bind_text(statement, 8, target_board, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
        sqlite3_step(statement) == SQLITE_DONE && sqlite3_changes(db) == 1;
    if (sqlite3_finalize(statement) != SQLITE_OK) valid = 0;
    return valid;
}

int wena_sqlite_checklist_move(sqlite3 *db, const WenaDomainCommand *command,
    const char *board, unsigned long *result_version)
{
    char card[65], target[65], checklist[65], number[32], target_board[WENA_ID_CAPACITY];
    unsigned long card_version, target_version, checklist_version, stored, position;
    ChecklistOrderRow *children, lists[WENA_CHECKLIST_MAX_PER_CARD];
    size_t count, total, list_count, index, expected_total, expected_lists;
    int valid;
    if (!db || !command || !result_version || sqlite3_get_autocommit(db) ||
        command->operation != WENA_DOMAIN_MOVE_CHECKLIST ||
        !wena_model_identifier_valid(board) ||
        !wena_mutation_text(command, "cardId", card, sizeof(card), 0) ||
        !wena_model_identifier_valid(card) ||
        !wena_mutation_text(command, "targetCardId", target, sizeof(target), 0) ||
        !wena_model_identifier_valid(target) || !strcmp(card, target) ||
        !wena_mutation_text(command, "checklistId", checklist, sizeof(checklist), 0) ||
        !wena_model_identifier_valid(checklist) ||
        !wena_mutation_text(command, "expectedVersion", number, sizeof(number), 0) ||
        !wena_mutation_decimal(number, 0, WENA_VERSION_MUTATE_MAX, &card_version) ||
        !wena_mutation_text(command, "expectedTargetVersion", number, sizeof(number), 0) ||
        !wena_mutation_decimal(number, 0, WENA_VERSION_MUTATE_MAX, &target_version) ||
        !wena_mutation_text(command, "expectedChecklistVersion", number, sizeof(number), 0) ||
        !wena_mutation_decimal(number, 0, WENA_VERSION_MUTATE_MAX, &checklist_version) ||
        !destination_board(command, board, target_board) ||
        !card_revision(db, board, card, card_version) ||
        !card_revision(db, target_board, target, target_version)) return 0;
    children = (ChecklistOrderRow *)calloc(WENA_CHECKLIST_MAX_ITEMS, sizeof(*children));
    if (!children) return 0;
    valid = 0;
    /* Validate both complete collections before writes. Include children whose
     * card_id was corrupted away from the selected checklist's source card. */
    if (!read_card_order(db, board, card, checklist, 1, children, &count, &stored,
            NULL) || stored != checklist_version ||
        !read_card_order(db, target_board, target, "", 0, lists, &list_count, &stored, &total) ||
        list_count >= WENA_CHECKLIST_MAX_PER_CARD ||
        count > WENA_CHECKLIST_MAX_ITEMS - total) goto done;
    expected_total = total + count;
    expected_lists = list_count + 1u;
    position = 0;
    if (list_count) {
        if (lists[list_count - 1].position == WENA_CHECKLIST_POSITION_MAX) goto done;
        position = lists[list_count - 1].position + 1UL;
    }
    for (index = 0; index < count; ++index)
        if (children[index].version > WENA_VERSION_MUTATE_MAX) goto done;
    /* The immutable v3 composite FK includes card_id. Defer enforcement only
     * within the caller-owned transaction, update parent and all children, then
     * let COMMIT check it. COMMIT/ROLLBACK reset defer_foreign_keys automatically.
     * No schema rewrite, delete/reinsert or disabled FK enforcement is needed. */
    if (sqlite3_exec(db, "PRAGMA defer_foreign_keys=ON", NULL, NULL, NULL) != SQLITE_OK)
        goto done;
    if (!move_row(db, "UPDATE checklists SET board_id=?8,card_id=?4,position=?7,version=version+1,"
        "updated_at=max(updated_at,CAST(strftime('%s','now') AS INTEGER)*1000) "
        "WHERE board_id=?1 AND card_id=?2 AND id=?3 AND id=?5 AND version=?6",
        board, card, checklist, target, checklist, checklist_version, position, target_board)) goto done;
    for (index = 0; index < count; ++index) {
        if (!move_row(db, "UPDATE checklist_items SET board_id=?8,card_id=?4,version=version+1,"
            "updated_at=max(updated_at,CAST(strftime('%s','now') AS INTEGER)*1000) "
            "WHERE board_id=?1 AND card_id=?2 AND checklist_id=?3 AND id=?5 "
            "AND version=?6 AND position=?7", board, card, checklist, target,
            children[index].id, children[index].version, children[index].position, target_board)) goto done;
    }
    /* Revalidate the changed collection even on a caller connection without FK
     * enforcement. Exact row updates reject ignored children; this read also
     * refuses a trigger leaving a child in the source or a malformed destination. */
    if (!read_card_order(db, target_board, target, checklist, 0, lists, &list_count, &stored,
        &total) || stored != checklist_version + 1UL ||
        total != expected_total || list_count != expected_lists) goto done;
    valid = advance_transfer_cards(db, board, card, card_version,
        target_board, target, target_version);
    if (valid) *result_version = card_version + 1UL;
done:
    free(children);
    return valid;
}

static int command_revision(const WenaDomainCommand *command, const char *key,
    unsigned long *version)
{
    char number[32];
    return wena_mutation_text(command, key, number, sizeof(number), 0) &&
        wena_mutation_decimal(number, 0, WENA_VERSION_MUTATE_MAX, version);
}

static int advance_checklist(sqlite3 *db, const char *board, const char *card,
    const char *checklist, unsigned long version)
{
    sqlite3_stmt *statement;
    int valid;
    if (!prepare(db, "UPDATE checklists SET version=version+1,updated_at="
        "max(updated_at,CAST(strftime('%s','now') AS INTEGER)*1000) WHERE "
        "board_id=?1 AND card_id=?2 AND id=?3 AND version=?4", board, card,
        checklist, &statement)) return 0;
    valid = sqlite3_bind_int64(statement, 4, (sqlite3_int64)version) == SQLITE_OK &&
        sqlite3_step(statement) == SQLITE_DONE && sqlite3_changes(db) == 1;
    if (sqlite3_finalize(statement) != SQLITE_OK) valid = 0;
    return valid;
}

int wena_sqlite_checklist_item_move(sqlite3 *db, const WenaDomainCommand *command,
    const char *board, unsigned long *result_version)
{
    char card[65], target[65], checklist[65], destination[65], item[65], target_board[WENA_ID_CAPACITY];
    unsigned long cv, tv, kv, dv, iv, stored, position;
    ChecklistOrderRow *rows;
    size_t count, source_count, source_total, destination_count, destination_total;
    size_t index, total;
    int same_card, valid;
    sqlite3_stmt *statement;
    if (!db || !command || !result_version || sqlite3_get_autocommit(db) ||
        command->operation != WENA_DOMAIN_MOVE_CHECKLIST_ITEM ||
        !wena_model_identifier_valid(board) ||
        !wena_mutation_text(command, "cardId", card, sizeof(card), 0) ||
        !wena_model_identifier_valid(card) ||
        !wena_mutation_text(command, "targetCardId", target, sizeof(target), 0) ||
        !wena_model_identifier_valid(target) ||
        !wena_mutation_text(command, "checklistId", checklist, sizeof(checklist), 0) ||
        !wena_model_identifier_valid(checklist) ||
        !wena_mutation_text(command, "targetChecklistId", destination, sizeof(destination), 0) ||
        !wena_model_identifier_valid(destination) || !strcmp(checklist, destination) ||
        !wena_mutation_text(command, "itemId", item, sizeof(item), 0) ||
        !wena_model_identifier_valid(item) ||
        !command_revision(command, "expectedVersion", &cv) ||
        !command_revision(command, "expectedTargetVersion", &tv) ||
        !command_revision(command, "expectedChecklistVersion", &kv) ||
        !command_revision(command, "expectedTargetChecklistVersion", &dv) ||
        !command_revision(command, "expectedItemVersion", &iv) ||
        !destination_board(command, board, target_board) ||
        !card_revision(db, board, card, cv) || !card_revision(db, target_board, target, tv)) return 0;
    same_card = !strcmp(card, target);
    rows = (ChecklistOrderRow *)calloc(WENA_CHECKLIST_MAX_ITEMS, sizeof(*rows));
    if (!rows) return 0;
    valid = 0;
    if (!read_card_order(db, board, card, checklist, 1, rows, &source_count, &stored,
        &source_total) || stored != kv) goto done;
    for (index = 0; index < source_count; ++index)
        if (!strcmp(rows[index].id, item)) break;
    if (index == source_count || rows[index].version != iv) goto done;
    if (!read_card_order(db, target_board, target, destination, 1, rows, &destination_count,
        &stored, &destination_total) || stored != dv ||
        (!same_card && destination_total == WENA_CHECKLIST_MAX_ITEMS)) goto done;
    position = 0;
    if (destination_count) {
        if (rows[destination_count - 1].position == WENA_CHECKLIST_POSITION_MAX) goto done;
        position = rows[destination_count - 1].position + 1UL;
    }
    /* Existing destination parents satisfy the immutable composite FK in the
     * same UPDATE. No deferred constraint or temporary detach is necessary. */
    if (!prepare(db, "UPDATE checklist_items SET board_id=?9,card_id=?4,checklist_id=?5,"
        "position=?6,version=version+1,updated_at=max(updated_at,"
        "CAST(strftime('%s','now') AS INTEGER)*1000) WHERE board_id=?1 AND "
        "card_id=?2 AND checklist_id=?3 AND id=?7 AND version=?8",
        board, card, checklist, &statement)) goto done;
    valid = sqlite3_bind_text(statement, 4, target, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
        sqlite3_bind_text(statement, 5, destination, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
        sqlite3_bind_int64(statement, 6, (sqlite3_int64)position) == SQLITE_OK &&
        sqlite3_bind_text(statement, 7, item, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
        sqlite3_bind_int64(statement, 8, (sqlite3_int64)iv) == SQLITE_OK &&
        sqlite3_bind_text(statement, 9, target_board, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
        sqlite3_step(statement) == SQLITE_DONE && sqlite3_changes(db) == 1;
    if (sqlite3_finalize(statement) != SQLITE_OK) valid = 0;
    if (!valid) goto done;
    valid = 0;
    if (!advance_checklist(db, board, card, checklist, kv) ||
        !advance_checklist(db, target_board, target, destination, dv)) goto done;
    /* Postconditions protect against ignored updates and trigger side effects,
     * including on test connections whose foreign-key enforcement is disabled. */
    if (!read_card_order(db, board, card, checklist, 1, rows, &count, &stored, &total) ||
        stored != kv + 1UL || count != source_count - 1u ||
        total != source_total - (same_card ? 0u : 1u)) goto done;
    if (!read_card_order(db, target_board, target, destination, 1, rows, &count, &stored, &total) ||
        stored != dv + 1UL || count != destination_count + 1u ||
        total != destination_total + (same_card ? 0u : 1u)) goto done;
    for (index = 0; index < count; ++index)
        if (!strcmp(rows[index].id, item)) break;
    if (index == count || rows[index].version != iv + 1UL ||
        rows[index].position != position) goto done;
    valid = advance_transfer_cards(db, board, card, cv, target_board, target, tv);
    if (valid) *result_version = cv + 1UL;
done:
    free(rows);
    return valid;
}
