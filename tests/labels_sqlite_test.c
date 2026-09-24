#include "../client/features/labels/panel.h"
#include "../client/features/labels/mutation.h"
#include <nuklear.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void sql(sqlite3 *database, const char *query)
{
    char *error;
    int result;
    error = NULL;
    result = sqlite3_exec(database, query, NULL, NULL, &error);
    if (result != SQLITE_OK) fprintf(stderr, "%s: %s\n", query, error);
    sqlite3_free(error);
    assert(result == SQLITE_OK);
}
static sqlite3_int64 number(sqlite3 *database, const char *query)
{
    sqlite3_stmt *statement;
    sqlite3_int64 result;
    assert(sqlite3_prepare_v2(database, query, -1, &statement, NULL) == SQLITE_OK);
    assert(sqlite3_step(statement) == SQLITE_ROW);
    result = sqlite3_column_int64(statement, 0);
    assert(sqlite3_finalize(statement) == SQLITE_OK);
    return result;
}
static void schema(sqlite3 *database, const char *path)
{
    FILE *file;
    long length;
    char *data;
    file = fopen(path, "rb");
    assert(file && !fseek(file, 0, SEEK_END));
    length = ftell(file);
    assert(length > 0);
    rewind(file);
    data = (char *)malloc((size_t)length + 1);
    assert(data && fread(data, 1, (size_t)length, file) == (size_t)length);
    data[length] = 0;
    assert(!fclose(file));
    sql(database, data);
    free(data);
}
static void frame(WenaLabelsState *state, WenaCard *card, const char *button,
    const char *name)
{
    struct nk_context context;
    memset(&context, 0, sizeof(context));
    context.button_to_press = button;
    context.edit_text = name;
    assert(wena_labels_render(&context, state, "b", card, card ? 1 : 0, 800, 600));
}
static void reopen(WenaLabelsState *state, WenaCard *card)
{
    wena_labels_close(state);
    assert(wena_labels_open(state, "b", card));
}
int main(int argc, char **argv)
{
    sqlite3 *database;
    WenaLabelMutation adapter;
    WenaLabelsState state;
    WenaLabelSnapshot *other;
    WenaLabelEdit edit;
    WenaCard card, wrong;
    WenaId label_id;
    char long_name[150], query[512];
    sqlite3_int64 keys, board_version, card_version, archived_version;
    unsigned long request;
    assert(argc == 5);
    assert(sqlite3_open(argv[4], &database) == SQLITE_OK);
    sql(database, "PRAGMA foreign_keys=ON");
    schema(database, argv[1]);
    schema(database, argv[2]);
    schema(database, argv[3]);
    sql(database, "INSERT INTO actors VALUES('u','User',1);"
        "INSERT INTO boards VALUES('b','Board',1),('other','Other',1);"
        "INSERT INTO lists VALUES('l','b','List',0,1),('ol','other','Other',0,1);"
        "INSERT INTO swimlanes VALUES('s','b','Lane',0,1),('os','other','Other',0,1);"
        "INSERT INTO cards VALUES('c','b','s','l','Card',0,0,1),"
        "('c2','b','s','l','Later archived',1,0,1),"
        "('foreign','other','os','ol','Foreign',0,0,1)");
    assert(wena_card_init(&card, "c", "b", "s", "l", "Card", 0, 0));
    assert(wena_label_mutation_init(&adapter, database, "u", "b"));
    wena_labels_init(&state, wena_label_mutation_load, wena_label_mutation_save, &adapter);
    assert(wena_labels_open(&state, "b", &card));
    frame(&state, &card, "Create Label", NULL);
    frame(&state, &card, "Cancel", "Cancelled");
    assert(number(database, "SELECT count(*) FROM labels") == 0);
    frame(&state, &card, "Create Label", NULL);
    frame(&state, &card, "Create", "");
    assert(!state.error && state.snapshot->label_count == 1 &&
        !state.snapshot->labels[0].name[0]);
    strcpy(label_id, state.snapshot->labels[0].id);
    frame(&state, &card, "Create Label", NULL);
    memset(long_name, 'x', sizeof(long_name));
    long_name[sizeof(long_name) - 1] = 0;
    frame(&state, &card, "Create", long_name);
    assert(state.error && number(database, "SELECT count(*) FROM labels") == 1);
    frame(&state, &card, "Create", "Bad\001name");
    assert(state.error && number(database, "SELECT count(*) FROM labels") == 1);
    frame(&state, &card, "Create", "  Other label  ");
    assert(!state.error && state.snapshot->label_count == 2 &&
        !strcmp(state.snapshot->labels[1].name, "Other label"));
    frame(&state, &card, "Change Label", NULL);
    strcpy(state.color_input.custom_color, "#001122");
    state.color_input.color_length = 7;
    state.color_input.use_custom_color = 1;
    frame(&state, &card, "Save", "Alpha");
    assert(!state.error && !strcmp(state.snapshot->labels[0].name, "Alpha") &&
        !strcmp(state.snapshot->labels[0].color, "#001122"));
    frame(&state, &card, "Alpha", NULL);
    assert(!state.error && state.snapshot->assigned[0] &&
        state.snapshot->assigned_card_counts[0] == 1);
    frame(&state, &card, "Alpha", NULL);
    assert(!state.error && !state.snapshot->assigned[0]);
    frame(&state, &card, "Alpha", NULL);
    assert(state.snapshot->assigned[0]);
    frame(&state, &card, "Change Label", NULL);
    keys = number(database, "SELECT count(*) FROM idempotency_keys");
    board_version = number(database, "SELECT version FROM boards WHERE id='b'");
    sql(database, "CREATE TRIGGER rollback_label BEFORE INSERT ON idempotency_keys "
        "BEGIN SELECT RAISE(ABORT,'rollback label'); END");
    frame(&state, &card, "Save", "After rollback");
    assert(state.error && state.action == WENA_LABEL_EDIT &&
        !strcmp(state.name, "After rollback") && !strcmp(state.snapshot->labels[0].name, "Alpha"));
    assert(number(database, "SELECT count(*) FROM labels WHERE name='After rollback'") == 0);
    assert(number(database, "SELECT version FROM boards WHERE id='b'") == board_version);
    assert(number(database, "SELECT count(*) FROM idempotency_keys") == keys);
    sql(database, "DROP TRIGGER rollback_label");
    frame(&state, &card, "Save", NULL);
    assert(!state.error && !strcmp(state.snapshot->labels[0].name, "After rollback"));
    /* Each captured revision remains authoritative until explicitly reopened. */
    frame(&state, &card, "Change Label", NULL);
    sql(database, "UPDATE boards SET version=version+1 WHERE id='b'");
    frame(&state, &card, "Save", "Stale board");
    assert(state.error && state.action && !strcmp(state.name, "Stale board"));
    reopen(&state, &card);
    frame(&state, &card, "Change Label", NULL);
    sprintf(query, "UPDATE labels SET version=version+1 WHERE board_id='b' AND id='%s'", label_id);
    sql(database, query);
    frame(&state, &card, "Save", "Stale label");
    assert(state.error && state.action);
    reopen(&state, &card);
    frame(&state, &card, "Change Label", NULL);
    sql(database, "UPDATE cards SET version=version+1 WHERE id='c'");
    frame(&state, &card, "Save", "Stale card");
    assert(state.error && state.action);
    reopen(&state, &card);
    /* A current payload cannot reuse an already committed request key. */
    memset(&edit, 0, sizeof(edit));
    edit.action = WENA_LABEL_CREATE;
    edit.expected_board_version = state.snapshot->board_version;
    edit.expected_card_version = state.snapshot->card_version;
    edit.name = "Replay";
    edit.color = "red";
    request = (unsigned long)number(database, "SELECT min(request_version) FROM idempotency_keys");
    assert(request && !wena_label_mutation_save_request(&adapter, "b", "c", &edit, request));
    assert(!wena_label_mutation_save(&adapter, "other", "c", &edit));
    assert(!wena_label_mutation_save(&adapter, "b", "foreign", &edit));
    assert(wena_card_init(&wrong, "foreign", "other", "os", "ol", "Foreign", 0, 0));
    assert(!wena_labels_open(&state, "b", &wrong));
    reopen(&state, &card);
    wena_labels_close(&state);
    assert(sqlite3_close(database) == SQLITE_OK);
    assert(sqlite3_open(argv[4], &database) == SQLITE_OK);
    sql(database, "PRAGMA foreign_keys=ON");
    assert(wena_label_mutation_init(&adapter, database, "u", "b"));
    assert(wena_labels_open(&state, "b", &card));
    assert(state.snapshot->label_count == 2 && state.snapshot->assigned[0] &&
        !strcmp(state.snapshot->labels[0].name, "After rollback") &&
        !strcmp(state.snapshot->labels[0].color, "#001122"));
    other = wena_label_snapshot_create();
    assert(other && wena_label_mutation_load(&adapter, "b", "c2", other));
    memset(&edit, 0, sizeof(edit));
    edit.action = WENA_LABEL_ASSIGN;
    edit.label_id = label_id;
    edit.expected_board_version = other->board_version;
    edit.expected_card_version = other->card_version;
    edit.expected_label_version = other->label_versions[0];
    assert(wena_label_mutation_save(&adapter, "b", "c2", &edit));
    wena_label_snapshot_free(other);
    sql(database, "UPDATE cards SET archived=1,version=version+1 WHERE id='c2'");
    reopen(&state, &card);
    frame(&state, &card, "Change Label", NULL);
    frame(&state, &card, "Delete", "Discarded destructive draft");
    assert(state.action == WENA_LABEL_DELETE && state.affected_cards == 2 &&
        !strcmp(state.name, "After rollback"));
    frame(&state, &card, "Cancel", NULL);
    assert(number(database, "SELECT count(*) FROM card_labels") == 2);
    frame(&state, &card, "Change Label", NULL);
    frame(&state, &card, "Delete", NULL);
    keys = number(database, "SELECT count(*) FROM idempotency_keys");
    card_version = number(database, "SELECT version FROM cards WHERE id='c'");
    archived_version = number(database, "SELECT version FROM cards WHERE id='c2'");
    sql(database, "CREATE TRIGGER rollback_delete BEFORE INSERT ON idempotency_keys "
        "BEGIN SELECT RAISE(ABORT,'rollback delete'); END");
    frame(&state, &card, "Delete", NULL);
    assert(state.error && state.action == WENA_LABEL_DELETE &&
        number(database, "SELECT count(*) FROM labels") == 2 &&
        number(database, "SELECT count(*) FROM card_labels") == 2 &&
        number(database, "SELECT version FROM cards WHERE id='c'") == card_version &&
        number(database, "SELECT version FROM cards WHERE id='c2'") == archived_version &&
        number(database, "SELECT count(*) FROM idempotency_keys") == keys);
    sql(database, "DROP TRIGGER rollback_delete");
    frame(&state, &card, "Delete", NULL);
    assert(!state.error && !state.action && state.snapshot->label_count == 1 &&
        !strcmp(state.snapshot->labels[0].name, "Other label"));
    assert(number(database, "SELECT count(*) FROM card_labels") == 0 &&
        number(database, "SELECT version FROM cards WHERE id='c'") == card_version + 1 &&
        number(database, "SELECT version FROM cards WHERE id='c2'") == archived_version + 1);
    wena_labels_close(&state);
    assert(sqlite3_close(database) == SQLITE_OK);
    assert(sqlite3_open(argv[4], &database) == SQLITE_OK);
    assert(wena_label_mutation_init(&adapter, database, "u", "b"));
    assert(wena_labels_open(&state, "b", NULL));
    assert(state.snapshot->label_count == 1 && !state.snapshot->card_version &&
        !strcmp(state.snapshot->labels[0].name, "Other label"));
    wena_labels_close(&state);
    assert(sqlite3_close(database) == SQLITE_OK);
    puts("Labels UI SQLite writes, cancel, revisions, scopes, replay, rollback and reopening passed");
    return 0;
}
