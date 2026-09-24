#include "checklist_mutation.h"
#include "../../models/checklist_item_titles.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

WenaChecklistSnapshot *wena_checklist_snapshot_create(void)
{
    return (WenaChecklistSnapshot *) calloc(1, sizeof(WenaChecklistSnapshot));
}

void wena_checklist_snapshot_free(WenaChecklistSnapshot *snapshot)
{
    free(snapshot);
}

static int scope_valid(WenaChecklistMutation *adapter, const char *board_id, const char *card_id)
{
    return adapter &&
        adapter->persistence.database &&
        wena_model_identifier_valid(board_id) &&
        wena_model_identifier_valid(card_id) &&
        !strcmp(board_id, adapter->board_id);
}

int wena_checklist_mutation_init(WenaChecklistMutation *adapter, sqlite3 *database,
    const char *actor_id, const char *board_id)
{
    if (!adapter) return 0;
    memset(adapter, 0, sizeof(*adapter));
    if (!database ||
        !wena_model_identifier_valid(actor_id) ||
        !wena_model_identifier_valid(board_id)) return 0;
    wena_sqlite_persistence_init(&adapter->persistence, database);
    strcpy(adapter->actor_id, actor_id);
    strcpy(adapter->board_id, board_id);
    sprintf(adapter->route, "/b/%s/native", board_id);
    return 1;
}

static int read_integer(sqlite3_stmt *statement, int column, unsigned long maximum,
    unsigned long *output)
{
    sqlite3_int64 stored_value;
    if (sqlite3_column_type(statement, column) != SQLITE_INTEGER) return 0;
    stored_value = sqlite3_column_int64(statement, column);
    if (stored_value < 0 || stored_value > (sqlite3_int64) maximum) return 0;
    *output = (unsigned long) stored_value;
    return 1;
}

/* Values are epoch milliseconds, stored using a second-precision SQLite clock.
* Keep them in SQLite integer width on 32-bit C89 platforms. */
static int timestamps_valid(sqlite3_stmt *statement, int first, int second)
{
    sqlite3_int64 created, updated;
    if (sqlite3_column_type(statement, first) != SQLITE_INTEGER ||
        sqlite3_column_type(statement, second) != SQLITE_INTEGER) return 0;
    created = sqlite3_column_int64(statement, first);
    updated = sqlite3_column_int64(statement, second);
    return created >= 0 && updated >= created;
}

static const char *read_text(sqlite3_stmt *statement, int column, size_t capacity)
{
    const char *value;
    int bytes;
    if (sqlite3_column_type(statement, column) != SQLITE_TEXT) return NULL;
    value = (const char *) sqlite3_column_text(statement, column);
    bytes = sqlite3_column_bytes(statement, column);
    if (!value ||
        bytes < 0 ||
        (size_t) bytes >= capacity ||
        memchr(value, 0, (size_t) bytes)) return NULL;
    return value;
}

static int prepare_scoped_query(WenaChecklistMutation *adapter, const char *sql,
    const char *board_id, const char *card_id, sqlite3_stmt **statement)
{
    if (sqlite3_prepare_v2(adapter->persistence.database, sql, -1, statement,
        NULL) != SQLITE_OK) return 0;
    if (sqlite3_bind_text(*statement, 1, board_id, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(*statement, 2, card_id, -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        sqlite3_finalize(*statement);
        *statement = NULL;
        return 0;
    }
    return 1;
}

int wena_checklist_mutation_load(void *context, const char *board_id, const char *card_id,
    WenaChecklistSnapshot *output)
{
    WenaChecklistMutation *adapter;
    WenaChecklistSnapshot *candidate;
    WenaChecklist *checklist;
    WenaChecklistItem *item;
    sqlite3_stmt *statement;
    const char *id, *title, *parent_id;
    unsigned long position, version, flag;
    size_t index, parent_index;
    int step, success;
    adapter = (WenaChecklistMutation *) context;
    if (!scope_valid(adapter, board_id, card_id) || !output) return 0;
    candidate = wena_checklist_snapshot_create();
    if (!candidate) return 0;
    statement = NULL;
    success = 0;
    if (sqlite3_exec(adapter->persistence.database, "BEGIN", NULL, NULL, NULL) != SQLITE_OK) {
        free(candidate);
        return 0;
    }
    if (!prepare_scoped_query(adapter,
        "SELECT version FROM cards WHERE board_id=?1 AND id=?2 AND archived=0 AND "
        "EXISTS(SELECT 1 FROM actors WHERE id=?3)", board_id, card_id, &statement)) goto done;
    sqlite3_bind_text(statement, 3, adapter->actor_id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(statement) != SQLITE_ROW ||
        !read_integer(statement, 0, WENA_VERSION_READ_MAX, &candidate->card_version) ||
        !candidate->card_version) goto done;
    sqlite3_finalize(statement);
    statement = NULL;
    strcpy(candidate->board_id, board_id);
    strcpy(candidate->card_id, card_id);
    /* Read all rows with this card ID, including corrupted foreign-board rows,
     * so scope validation cannot silently omit part of the snapshot. */
    if (!prepare_scoped_query(adapter,
        "SELECT id,title,position,version,hide_checked_items,hide_all_items,"
        "show_on_minicard,created_at,updated_at,board_id FROM checklists WHERE "
        "card_id=?2 ORDER BY position,id", board_id, card_id, &statement)) goto done;
    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        if (candidate->checklist_count >= WENA_CARD_CHECKLIST_CAPACITY) goto done;
        if (!read_text(statement, 9, 65) || strcmp(read_text(statement, 9, 65), board_id)) goto done;
        index = candidate->checklist_count;
        checklist = &candidate->checklists[index];
        id = read_text(statement, 0, 65);
        title = read_text(statement, 1, 129);
        if (!read_integer(statement, 2, WENA_CHECKLIST_POSITION_MAX, &position) ||
            !read_integer(statement, 3, WENA_VERSION_READ_MAX, &version) ||
            !version ||
            !timestamps_valid(statement, 7, 8) ||
            !wena_checklist_init(checklist, id, board_id, card_id, title, position)) goto done;
        if (index && candidate->checklists[index - 1].position >= position) goto done;
        if (!read_integer(statement, 4, 1, &flag)) goto done;
        checklist->hide_checked_items = (int) flag;
        if (!read_integer(statement, 5, 1, &flag)) goto done;
        checklist->hide_all_items = (int) flag;
        if (sqlite3_column_type(statement, 6) != SQLITE_NULL) {
            if (!read_integer(statement, 6, 1, &flag)) goto done;
            checklist->show_on_minicard = (WenaChecklistMinicard) flag;
        }
        candidate->checklist_versions[index] = version;
        ++candidate->checklist_count;
    }
    if (step != SQLITE_DONE) goto done;
    sqlite3_finalize(statement);
    statement = NULL;
    if (!prepare_scoped_query(adapter,
        "SELECT id,checklist_id,title,position,is_finished,version,created_at,"
        "updated_at,board_id FROM checklist_items WHERE card_id=?2 ORDER BY "
        "checklist_id,position,id", board_id, card_id, &statement)) goto done;
    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        if (candidate->item_count >= WENA_CARD_CHECKLIST_ITEM_CAPACITY) goto done;
        if (!read_text(statement, 8, 65) || strcmp(read_text(statement, 8, 65), board_id)) goto done;
        index = candidate->item_count;
        item = &candidate->items[index];
        id = read_text(statement, 0, 65);
        parent_id = read_text(statement, 1, 65);
        title = read_text(statement, 2, 129);
        if (!read_integer(statement, 3, WENA_CHECKLIST_POSITION_MAX, &position) ||
            !read_integer(statement, 4, 1, &flag) ||
            !read_integer(statement, 5, WENA_VERSION_READ_MAX, &version) ||
            !version ||
            !timestamps_valid(statement, 6, 7) ||
            !wena_checklist_item_init(item, id, board_id, card_id, parent_id,
                title, position, (int) flag)) goto done;
        for (parent_index = 0; parent_index < candidate->checklist_count;
             ++parent_index) {
            if (wena_checklist_item_validate_parent(item,
                    &candidate->checklists[parent_index])) break;
        }
        if (parent_index == candidate->checklist_count) goto done;
        if (index &&
            !strcmp(candidate->items[index - 1].checklist_id, parent_id) &&
            candidate->items[index - 1].position >= position) goto done;
        candidate->item_versions[index] = version;
        ++candidate->item_count;
    }
    if (step != SQLITE_DONE) goto done;
    sqlite3_finalize(statement);
    statement = NULL;
    if (!wena_checklist_snapshot_valid(candidate, board_id, card_id)) goto done;
    if (sqlite3_exec(adapter->persistence.database, "COMMIT", NULL, NULL, NULL) != SQLITE_OK) goto done;
    *output = *candidate;
    success = 1;
done:
    if (statement) sqlite3_finalize(statement);
    if (!success) sqlite3_exec(adapter->persistence.database, "ROLLBACK", NULL, NULL, NULL);
    free(candidate);
    return success;
}

static const char *map_operation(WenaChecklistAction action, WenaDomainOperation *domain)
{
    switch (action) {
    case WENA_CHECKLIST_CREATE:
        *domain = WENA_DOMAIN_CREATE_CHECKLIST;
        return "create-checklist";
    case WENA_CHECKLIST_RENAME:
        *domain = WENA_DOMAIN_RENAME_CHECKLIST;
        return "rename-checklist";
    case WENA_CHECKLIST_ADD_ITEM:
        *domain = WENA_DOMAIN_ADD_CHECKLIST_ITEM;
        return "add-checklist-item";
    case WENA_CHECKLIST_ADD_ITEMS:
        *domain = WENA_DOMAIN_ADD_CHECKLIST_ITEMS;
        return "add-checklist-items";
    case WENA_CHECKLIST_MOVE_ITEM:
        *domain = WENA_DOMAIN_MOVE_CHECKLIST_ITEM;
        return "move-checklist-item";
    case WENA_CHECKLIST_MOVE:
        *domain = WENA_DOMAIN_MOVE_CHECKLIST;
        return "move-checklist";
    case WENA_CHECKLIST_REORDER:
        *domain = WENA_DOMAIN_REORDER_CHECKLIST;
        return "reorder-checklist";
    case WENA_CHECKLIST_REORDER_ITEM:
        *domain = WENA_DOMAIN_REORDER_CHECKLIST_ITEM;
        return "reorder-checklist-item";
    case WENA_CHECKLIST_RENAME_ITEM:
        *domain = WENA_DOMAIN_RENAME_CHECKLIST_ITEM;
        return "rename-checklist-item";
    case WENA_CHECKLIST_SET_FINISHED:
        *domain = WENA_DOMAIN_SET_CHECKLIST_ITEM_FINISHED;
        return "set-checklist-item-finished";
    case WENA_CHECKLIST_SET_FLAGS:
        *domain = WENA_DOMAIN_SET_CHECKLIST_FLAGS;
        return "set-checklist-flags";
    case WENA_CHECKLIST_DELETE:
        *domain = WENA_DOMAIN_DELETE_CHECKLIST;
        return "delete-checklist";
    case WENA_CHECKLIST_DELETE_ITEM:
        *domain = WENA_DOMAIN_DELETE_CHECKLIST_ITEM;
        return "delete-checklist-item";
    default:
        return NULL;
    }
}

int wena_checklist_mutation_save_request(WenaChecklistMutation *adapter,
    const char *board_id, const char *card_id, const WenaChecklistEdit *edit,
    unsigned long request_version)
{
    WenaDomainCommand command;
    WenaRegionResponse response;
    char encoded[WENA_CHECKLIST_BATCH_MAX_BYTES * 3u + 1u];
    char batch[WENA_CHECKLIST_BATCH_MAX_ITEMS][WENA_CHECKLIST_TITLE_CAPACITY];
    const char *input;
    const char *checklist, *item;
    const char hex[] = "0123456789ABCDEF";
    size_t index, length, count;
    unsigned char byte;
    if (!scope_valid(adapter, board_id, card_id) ||
        !edit ||
        !request_version ||
        request_version >= (unsigned long) LONG_MAX ||
        !edit->expected_card_version ||
        edit->expected_card_version > WENA_VERSION_MUTATE_MAX) return 0;
    memset(&command, 0, sizeof(command));
    if (!map_operation(edit->action, &command.operation)) return 0;
    checklist = edit->checklist_id;
    item = edit->item_id;
    if (edit->action != WENA_CHECKLIST_CREATE &&
        (!wena_model_identifier_valid(checklist) ||
        !edit->expected_checklist_version ||
        edit->expected_checklist_version > WENA_VERSION_MUTATE_MAX)) return 0;
    if ((edit->action == WENA_CHECKLIST_RENAME_ITEM ||
        edit->action == WENA_CHECKLIST_SET_FINISHED ||
        edit->action == WENA_CHECKLIST_REORDER_ITEM ||
        edit->action == WENA_CHECKLIST_MOVE_ITEM ||
        edit->action == WENA_CHECKLIST_DELETE_ITEM) &&
        (!wena_model_identifier_valid(item) ||
        !edit->expected_item_version ||
        edit->expected_item_version > WENA_VERSION_MUTATE_MAX)) return 0;
    if (edit->action == WENA_CHECKLIST_MOVE_ITEM) {
        if (!wena_model_identifier_valid(edit->target_card_id) ||
            !wena_model_identifier_valid(edit->target_checklist_id) ||
            !strcmp(checklist, edit->target_checklist_id) ||
            !edit->expected_target_card_version ||
            edit->expected_target_card_version > WENA_VERSION_MUTATE_MAX ||
            !edit->expected_target_checklist_version ||
            edit->expected_target_checklist_version > WENA_VERSION_MUTATE_MAX) return 0;
        command.request_version = request_version;
        strcpy(command.user_id, adapter->actor_id);
        strcpy(command.route, adapter->route);
        sprintf(command.form_body,
            "cardId=%s&expectedVersion=%lu&checklistId=%s&expectedChecklistVersion=%lu&"
            "itemId=%s&expectedItemVersion=%lu&targetCardId=%s&expectedTargetVersion=%lu&"
            "targetChecklistId=%s&expectedTargetChecklistVersion=%lu", card_id,
            edit->expected_card_version, checklist, edit->expected_checklist_version,
            item, edit->expected_item_version, edit->target_card_id,
            edit->expected_target_card_version, edit->target_checklist_id,
            edit->expected_target_checklist_version);
        command.form_body_length = strlen(command.form_body);
        return wena_sqlite_persistence_apply(&adapter->persistence, &command, &response);
    }
    if (edit->action == WENA_CHECKLIST_MOVE) {
        if (!wena_model_identifier_valid(edit->target_card_id) ||
            !strcmp(card_id, edit->target_card_id) ||
            !edit->expected_target_card_version ||
            edit->expected_target_card_version > WENA_VERSION_MUTATE_MAX) return 0;
        command.request_version = request_version;
        strcpy(command.user_id, adapter->actor_id);
        strcpy(command.route, adapter->route);
        sprintf(command.form_body,
            "cardId=%s&expectedVersion=%lu&checklistId=%s&expectedChecklistVersion=%lu&"
            "targetCardId=%s&expectedTargetVersion=%lu", card_id,
            edit->expected_card_version, checklist, edit->expected_checklist_version,
            edit->target_card_id, edit->expected_target_card_version);
        command.form_body_length = strlen(command.form_body);
        return wena_sqlite_persistence_apply(&adapter->persistence, &command, &response);
    }
    if (edit->action == WENA_CHECKLIST_REORDER ||
        edit->action == WENA_CHECKLIST_REORDER_ITEM) {
        if (edit->target_position >= (edit->action == WENA_CHECKLIST_REORDER ?
            WENA_CARD_CHECKLIST_CAPACITY : WENA_CARD_CHECKLIST_ITEM_CAPACITY)) return 0;
        command.request_version = request_version;
        strcpy(command.user_id, adapter->actor_id); strcpy(command.route, adapter->route);
        sprintf(command.form_body,
            "cardId=%s&expectedVersion=%lu&checklistId=%s&expectedChecklistVersion=%lu&"
            "itemId=%s&expectedItemVersion=%lu&targetPosition=%lu", card_id,
            edit->expected_card_version, checklist, edit->expected_checklist_version,
            edit->action == WENA_CHECKLIST_REORDER_ITEM ? item : "",
            edit->expected_item_version, edit->target_position);
        command.form_body_length = strlen(command.form_body);
        return wena_sqlite_persistence_apply(&adapter->persistence, &command, &response);
    }
    if (edit->action == WENA_CHECKLIST_ADD_ITEMS) {
        if (!wena_checklist_item_batch_parse(edit->batch_text, edit->batch_length,
            batch, &count)) return 0;
        input = edit->batch_text;
        length = edit->batch_length;
    } else {
        input = NULL;
        length = 0;
    }
    if (edit->action == WENA_CHECKLIST_DELETE ||
        edit->action == WENA_CHECKLIST_DELETE_ITEM) {
        encoded[0] = 0;
    } else if (edit->action == WENA_CHECKLIST_SET_FINISHED) {
        if (edit->is_finished != 0 && edit->is_finished != 1) return 0;
        encoded[0] = 0;
    } else if (edit->action == WENA_CHECKLIST_SET_FLAGS) {
        if ((edit->hide_checked_items != 0 &&
            edit->hide_checked_items != 1) ||
            (edit->hide_all_items != 0 &&
            edit->hide_all_items != 1) ||
            (edit->show_on_minicard != WENA_CHECKLIST_MINICARD_INHERIT &&
            edit->show_on_minicard != WENA_CHECKLIST_MINICARD_HIDE &&
            edit->show_on_minicard != WENA_CHECKLIST_MINICARD_SHOW)) return 0;
        encoded[0] = 0;
    } else if (edit->action != WENA_CHECKLIST_ADD_ITEMS) {
        if (!wena_model_title_string_valid(edit->title, 129)) return 0;
        input = edit->title;
        length = strlen(edit->title);
    }
    if (input) {
        for (index = 0; index < length; ++index) {
            byte = (unsigned char) input[index];
            encoded[index * 3] = '%';
            encoded[index * 3 + 1] = hex[byte >> 4];
            encoded[index * 3 + 2] = hex[byte & 15];
        }
        encoded[length * 3] = 0;
    }
    command.request_version = request_version;
    strcpy(command.user_id, adapter->actor_id);
    strcpy(command.route, adapter->route);
    if (edit->action == WENA_CHECKLIST_ADD_ITEMS) {
        /* Worst case: 3093 encoded bytes + two 64-byte IDs, two 20-digit
         * versions and fixed keys, safely below the unchanged 4097-byte body. */
        sprintf(command.form_body,
            "cardId=%s&expectedVersion=%lu&checklistId=%s&"
            "expectedChecklistVersion=%lu&titles=%s", card_id,
            edit->expected_card_version, checklist,
            edit->expected_checklist_version, encoded);
        command.form_body_length = strlen(command.form_body);
        return wena_sqlite_persistence_apply(&adapter->persistence, &command, &response);
    }
    sprintf(command.form_body,
        "cardId=%s&expectedVersion=%lu&checklistId=%s&expectedChecklistVersion=%lu&"
        "itemId=%s&expectedItemVersion=%lu&title=%s&isFinished=%d", card_id,
        edit->expected_card_version, edit->action == WENA_CHECKLIST_CREATE ? "" : checklist,
        edit->expected_checklist_version, (edit->action == WENA_CHECKLIST_RENAME_ITEM ||
        edit->action == WENA_CHECKLIST_SET_FINISHED ||
        edit->action == WENA_CHECKLIST_DELETE_ITEM) ? item : "", edit->expected_item_version,
        encoded, edit->is_finished);
    if (edit->action == WENA_CHECKLIST_SET_FLAGS) {
        sprintf(command.form_body + strlen(command.form_body),
            "&hideChecked=%d&hideAll=%d&showOnMinicard=%d",
            edit->hide_checked_items, edit->hide_all_items,
            (int) edit->show_on_minicard);
    }
    command.form_body_length = strlen(command.form_body);
    return wena_sqlite_persistence_apply(&adapter->persistence, &command, &response);
}

int wena_checklist_mutation_save(void *context, const char *board_id, const char *card_id,
    const WenaChecklistEdit *edit)
{
    WenaChecklistMutation *adapter;
    WenaDomainOperation domain;
    const char *operation_name;
    sqlite3_stmt *statement;
    sqlite3_int64 request_version;
    int success;
    adapter = (WenaChecklistMutation *) context;
    if (!scope_valid(adapter, board_id, card_id) ||
        !edit ||
        (operation_name = map_operation(edit->action, &domain)) == NULL) return 0;
    if (sqlite3_prepare_v2(adapter->persistence.database,
        "SELECT COALESCE(max(request_version),0) FROM idempotency_keys WHERE "
        "actor_id=?1 AND route=?2 AND operation=?3", -1, &statement, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(statement, 1, adapter->actor_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, adapter->route, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, operation_name, -1, SQLITE_TRANSIENT);
    success = sqlite3_step(statement) == SQLITE_ROW &&
        sqlite3_column_type(statement, 0) == SQLITE_INTEGER;
    request_version = success ? sqlite3_column_int64(statement, 0) : -1;
    sqlite3_finalize(statement);
    if (request_version < 0 || request_version >= LONG_MAX - 1) return 0;
    return wena_checklist_mutation_save_request(adapter, board_id, card_id,
        edit, (unsigned long) request_version + 1);
}

int wena_checklist_mutation_complete(WenaChecklistMutation *adapter,
    WenaChecklistCompletionIntent *intent)
{
    WenaChecklistEdit edit;
    if (!intent || !intent->pending) return 0;
    intent->pending = 0;
    memset(&edit, 0, sizeof(edit));
    edit.action = WENA_CHECKLIST_SET_FINISHED;
    edit.checklist_id = intent->checklist_id; edit.item_id = intent->item_id;
    edit.expected_card_version = intent->card_version;
    edit.expected_checklist_version = intent->checklist_version;
    edit.expected_item_version = intent->item_version;
    edit.is_finished = intent->is_finished;
    return wena_checklist_mutation_save(adapter, intent->board_id,
        intent->card_id, &edit) ? 1 : -1;
}
