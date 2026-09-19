#include "checklist_batch.h"
#include "common.h"
#include "../sha256.h"
#include "../../models/checklist_item_titles.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

static int bind_scope(sqlite3_stmt *statement, const char *board,
    const char *card, const char *checklist)
{
    return sqlite3_bind_text(statement, 1, board, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
        sqlite3_bind_text(statement, 2, card, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
        sqlite3_bind_text(statement, 3, checklist, -1, SQLITE_TRANSIENT) == SQLITE_OK;
}

int wena_sqlite_checklist_batch(sqlite3 *db, const WenaDomainCommand *command,
    const char *board, unsigned long *result_version)
{
    char card[65], checklist[65], number[32];
    char input[WENA_CHECKLIST_BATCH_MAX_BYTES + 1u];
    char titles[WENA_CHECKLIST_BATCH_MAX_ITEMS][WENA_CHECKLIST_TITLE_CAPACITY];
    char base[65], identity[96], id[65];
    unsigned long card_version, checklist_version;
    size_t count, index;
    sqlite3_int64 maximum, total;
    sqlite3_stmt *statement;
    int valid;
    if (!db || !command || !result_version ||
        command->operation != WENA_DOMAIN_ADD_CHECKLIST_ITEMS || sqlite3_get_autocommit(db) ||
        !wena_model_identifier_valid(board)) return 0;
    if (!wena_mutation_text(command, "cardId", card, sizeof(card), 0) ||
        !wena_model_identifier_valid(card) ||
        !wena_mutation_text(command, "checklistId", checklist, sizeof(checklist), 0) ||
        !wena_model_identifier_valid(checklist) ||
        !wena_mutation_text(command, "expectedVersion", number, sizeof(number), 0) ||
        !wena_mutation_decimal(number, 0, WENA_VERSION_MUTATE_MAX,
            &card_version) ||
        !wena_mutation_text(command, "expectedChecklistVersion", number,
            sizeof(number), 0) ||
        !wena_mutation_decimal(number, 0, WENA_VERSION_MUTATE_MAX,
            &checklist_version) ||
        !wena_mutation_text(command, "titles", input, sizeof(input), 1) ||
        !wena_checklist_item_batch_parse(input, strlen(input), titles, &count))
        return 0;
    statement = NULL;
    if (sqlite3_prepare_v2(db,
        "SELECT c.version,k.version FROM cards c JOIN checklists k ON "
        "k.board_id=c.board_id AND k.card_id=c.id WHERE c.board_id=?1 "
        "AND c.id=?2 AND k.id=?3 AND c.archived=0", -1,
        &statement, NULL) != SQLITE_OK) return 0;
    valid = bind_scope(statement, board, card, checklist) &&
        sqlite3_step(statement) == SQLITE_ROW &&
        sqlite3_column_type(statement, 0) == SQLITE_INTEGER &&
        sqlite3_column_type(statement, 1) == SQLITE_INTEGER &&
        sqlite3_column_int64(statement, 0) == (sqlite3_int64)card_version &&
        sqlite3_column_int64(statement, 1) == (sqlite3_int64)checklist_version;
    sqlite3_finalize(statement);
    if (!valid) return 0;
    /* Include scope-corrupt rows even when a caller disabled foreign keys.
     * Capacity belongs to the complete card, position to this checklist. */
    if (sqlite3_prepare_v2(db,
        "SELECT count(*),COALESCE(max(CASE WHEN checklist_id=?3 THEN position "
        "END),-1),count(CASE WHEN board_id=?1 AND card_id=?2 AND "
        "typeof(position)='integer' AND position>=0 AND position<=2147483647 "
        "THEN 1 END) FROM checklist_items WHERE card_id=?2 OR checklist_id=?3",
        -1, &statement, NULL) != SQLITE_OK) return 0;
    valid = bind_scope(statement, board, card, checklist) &&
        sqlite3_step(statement) == SQLITE_ROW &&
        sqlite3_column_type(statement, 0) == SQLITE_INTEGER &&
        sqlite3_column_type(statement, 1) == SQLITE_INTEGER &&
        sqlite3_column_type(statement, 2) == SQLITE_INTEGER;
    total = valid ? sqlite3_column_int64(statement, 0) : -1;
    maximum = valid ? sqlite3_column_int64(statement, 1) : -2;
    valid = valid && total >= 0 && total == sqlite3_column_int64(statement, 2) &&
        total <= (sqlite3_int64)WENA_CHECKLIST_MAX_ITEMS - (sqlite3_int64)count &&
        maximum >= -1 && maximum <= (sqlite3_int64)WENA_CHECKLIST_POSITION_MAX -
            (sqlite3_int64)count;
    sqlite3_finalize(statement);
    if (!valid) return 0;
    wena_mutation_identity(command, "add-checklist-items", base);
    if (sqlite3_prepare_v2(db,
        "INSERT INTO checklist_items(id,board_id,card_id,checklist_id,title,"
        "position,is_finished,version,created_at,updated_at) VALUES(?4,?1,?2,"
        "?3,?5,?6,0,1,CAST(strftime('%s','now') AS INTEGER)*1000,"
        "CAST(strftime('%s','now') AS INTEGER)*1000)", -1,
        &statement, NULL) != SQLITE_OK) return 0;
    valid = 1;
    for (index = 0; index < count && valid; ++index) {
        sprintf(identity, "%s:%lu", base, (unsigned long)index);
        wena_sha256_hex((const unsigned char *)identity, strlen(identity), id);
        valid = sqlite3_reset(statement) == SQLITE_OK &&
            sqlite3_clear_bindings(statement) == SQLITE_OK &&
            bind_scope(statement, board, card, checklist) &&
            sqlite3_bind_text(statement, 4, id, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
            sqlite3_bind_text(statement, 5, titles[index], -1, SQLITE_TRANSIENT) == SQLITE_OK &&
            sqlite3_bind_int64(statement, 6, maximum + 1 + (sqlite3_int64)index) == SQLITE_OK &&
            sqlite3_step(statement) == SQLITE_DONE && sqlite3_changes(db) == 1;
    }
    sqlite3_finalize(statement);
    if (!valid) return 0;
    if (sqlite3_prepare_v2(db,
        "UPDATE checklists SET version=version+1,updated_at=max(updated_at,"
        "CAST(strftime('%s','now') AS INTEGER)*1000) WHERE board_id=?1 AND "
        "card_id=?2 AND id=?3 AND version=?4", -1, &statement, NULL) != SQLITE_OK)
        return 0;
    valid = bind_scope(statement, board, card, checklist) &&
        sqlite3_bind_int64(statement, 4, (sqlite3_int64)checklist_version) == SQLITE_OK &&
        sqlite3_step(statement) == SQLITE_DONE && sqlite3_changes(db) == 1;
    sqlite3_finalize(statement);
    if (!valid) return 0;
    if (sqlite3_prepare_v2(db,
        "UPDATE cards SET version=version+1 WHERE board_id=?1 AND id=?2 "
        "AND version=?3 AND archived=0", -1, &statement, NULL) != SQLITE_OK) return 0;
    valid = sqlite3_bind_text(statement, 1, board, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
        sqlite3_bind_text(statement, 2, card, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
        sqlite3_bind_int64(statement, 3, (sqlite3_int64)card_version) == SQLITE_OK &&
        sqlite3_step(statement) == SQLITE_DONE && sqlite3_changes(db) == 1;
    sqlite3_finalize(statement);
    if (valid) *result_version = card_version + 1UL;
    return valid;
}
