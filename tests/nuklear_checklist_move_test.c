#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_IMPLEMENTATION
#include <nuklear.h>
#include "../client/features/checklists.h"
#include "../client/features/checklist_mutation.h"
#include "../imports/ui/page_contract.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int writes, reads, fail_reload;
static int load(void *ctx, const char *board, const char *card, WenaChecklistSnapshot *out)
{
    ++reads;
    if (fail_reload && !strcmp(card, "src")) return 0;
    return wena_checklist_mutation_load(ctx, board, card, out);
}
static int save(void *ctx, const char *board, const char *card, const WenaChecklistEdit *edit)
{
    ++writes;
    return wena_checklist_mutation_save(ctx, board, card, edit);
}
static void sql(sqlite3 *db, const char *query)
{ assert(sqlite3_exec(db, query, NULL, NULL, NULL) == SQLITE_OK); }
static void schema(sqlite3 *db, const char *path)
{
    FILE *f; long n; char *s;
    f = fopen(path, "rb"); assert(f); assert(!fseek(f, 0, SEEK_END));
    n = ftell(f); assert(n > 0); rewind(f); s = (char *)malloc((size_t)n + 1u); assert(s);
    assert(fread(s, 1, (size_t)n, f) == (size_t)n); s[n] = 0;
    assert(!fclose(f)); sql(db, s); free(s);
}
static float text_width(nk_handle handle, float height,
                        const char *text, int length)
{
    (void)handle; (void)text;
    return height * (float)length * 0.5f;
}

static void render(struct nk_context *ctx,WenaChecklistsState *state,WenaCard *card)
{
    const struct nk_command *command;
    if(state->visible) assert(wena_checklists_render(ctx,state,card,5,1000,800));
    nk_foreach(command,ctx) { (void)command; }
}
static void character(struct nk_context *ctx,WenaChecklistsState *state,WenaCard *card,nk_rune rune)
{ nk_clear(ctx);nk_input_begin(ctx);nk_input_unicode(ctx,rune);nk_input_end(ctx);render(ctx,state,card); }
static void key(struct nk_context *ctx,WenaChecklistsState *state,WenaCard *card,enum nk_keys key,int down)
{ nk_clear(ctx);nk_input_begin(ctx);nk_input_key(ctx,key,down);nk_input_end(ctx);render(ctx,state,card); }
/* Locate rendered text, avoiding assumptions about font-specific button widths. */
static struct nk_vec2 label_center(struct nk_context *ctx, const char *label)
{
    const struct nk_command *command;
    const struct nk_command_text *text;
    nk_foreach(command, ctx) {
        if (command->type != NK_COMMAND_TEXT) continue;
        text = (const struct nk_command_text *)command;
        if ((size_t)text->length == strlen(label) &&
            memcmp(text->string, label, (size_t)text->length) == 0)
            return nk_vec2((float)text->x + (float)text->w * 0.5f,
                           (float)text->y + (float)text->h * 0.5f);
    }
    fprintf(stderr, "missing rendered control: %s\n", label);
    assert(0);
    return nk_vec2(0.0f, 0.0f);
}

static void click_at(struct nk_context *ctx, WenaChecklistsState *state,
                     WenaCard *card, struct nk_vec2 point)
{
    int down;
    for (down = 1; down >= 0; --down) {
        nk_clear(ctx);
        nk_input_begin(ctx);
        nk_input_motion(ctx, (int)point.x, (int)point.y);
        nk_input_button(ctx, NK_BUTTON_LEFT, (int)point.x, (int)point.y, down);
        nk_input_end(ctx);
        render(ctx, state, card);
    }
}

static void click(struct nk_context *ctx, WenaChecklistsState *state,
                  WenaCard *card, const char *label)
{
    click_at(ctx, state, card, label_center(ctx, label));
}

static void open_move(struct nk_context *ctx, WenaChecklistsState *state, WenaCard *cards)
{
    character(ctx, state, cards, 0);
    click(ctx, state, cards, "Rename"); character(ctx, state, cards, 0);
    strcpy(state->input, "Unsaved rename"); state->length = (int)strlen(state->input);
    click(ctx, state, cards, "Move Checklist"); character(ctx, state, cards, 0);
    assert(state->action == WENA_CHECKLIST_MOVE && !strcmp(state->input, "Tasks"));
}
static void destination(struct nk_context *ctx, WenaChecklistsState *state, WenaCard *cards)
{
    click(ctx, state, cards, "Cards"); character(ctx, state, cards, 0);
    click(ctx, state, cards, "Same title [dst]"); character(ctx, state, cards, 0);
    assert(!strcmp(state->target_card_id, "dst") && state->target_card_version);
}
int main(int argc, char **argv)
{
    struct nk_context ctx;
    struct nk_user_font font;
    WenaChecklistsState state;
    WenaChecklistMutation adapter;
    WenaChecklistSnapshot *snapshot;
    WenaCard cards[5];
    sqlite3 *db;
    int previous, previous_reads;
    assert(argc == 5); assert(sqlite3_open(argv[4], &db) == SQLITE_OK);
    sql(db, "PRAGMA foreign_keys=ON"); schema(db, argv[1]); schema(db, argv[2]); schema(db, argv[3]);
    sql(db, "INSERT INTO actors VALUES('u','User',1);INSERT INTO boards VALUES('b','Board',1);"
        "INSERT INTO lists VALUES('l','b','List',0,1);INSERT INTO swimlanes VALUES('s','b','Lane',0,1);"
        "INSERT INTO cards VALUES('src','b','s','l','Source',0,0,1);"
        "INSERT INTO cards VALUES('dst','b','s','l','Same title',1,0,1);"
        "INSERT INTO cards VALUES('another','b','s','l','Same title',2,0,1)");
    sql(db, "INSERT INTO checklists(id,board_id,card_id,title,position) VALUES('cl','b','src','Tasks',8);"
        "INSERT INTO checklist_items(id,board_id,card_id,checklist_id,title,position,is_finished) "
        "VALUES('i','b','src','cl','Completed',19,1)");
    assert(wena_card_init(&cards[0], "src", "b", "s", "l", "Source", 0, 0));
    assert(wena_card_init(&cards[1], "dst", "b", "s", "l", "Same title", 1, 0));
    assert(wena_card_init(&cards[2], "another", "b", "s", "l", "Same title", 2, 0));
    assert(wena_card_init(&cards[3], "archived", "b", "s", "l", "Archived", 3, 1));
    assert(wena_card_init(&cards[4], "foreign", "other", "s", "l", "Foreign", 0, 0));
    assert(wena_checklist_mutation_init(&adapter, db, "u", "b"));
    wena_checklists_init(&state, load, save, &adapter);
    memset(&font, 0, sizeof(font)); font.height = 13; font.width = text_width;
    assert(nk_init_default(&ctx, &font));
    assert(wena_checklists_open(&state, cards)); render(&ctx, &state, cards);
    open_move(&ctx, &state, cards);
    click(&ctx, &state, cards, "Save"); assert(state.error && !writes);
    destination(&ctx, &state, cards); previous_reads = reads;
    character(&ctx, &state, cards, 0); character(&ctx, &state, cards, 0);
    assert(reads == previous_reads);
    key(&ctx, &state, cards, NK_KEY_ENTER, 1); key(&ctx, &state, cards, NK_KEY_ENTER, 0);
    assert(!writes && state.action == WENA_CHECKLIST_MOVE);
    click(&ctx, &state, cards, "Cancel"); assert(!state.action && !writes);
    open_move(&ctx, &state, cards); destination(&ctx, &state, cards);
    key(&ctx, &state, cards, NK_KEY_TEXT_RESET_MODE, 1);
    assert(!state.visible && !state.snapshot && !writes);
    key(&ctx, &state, cards, NK_KEY_TEXT_RESET_MODE, 0);
    assert(wena_checklists_open(&state, cards));
    open_move(&ctx, &state, cards); destination(&ctx, &state, cards);
    /* Stale destination refuses; neither an idle frame nor Save refreshes it. */
    sql(db, "UPDATE cards SET version=version+1 WHERE id='dst'");
    click(&ctx, &state, cards, "Save");
    assert(state.error && state.action == WENA_CHECKLIST_MOVE && state.target_card_version == 1);
    assert(state.snapshot->checklist_count == 1 && state.snapshot->item_count == 1);
    click(&ctx, &state, cards, "Cancel");
    open_move(&ctx, &state, cards); destination(&ctx, &state, cards);
    assert(state.target_card_version == 2);
    /* A newly archived or removed cached destination invalidates selection. */
    cards[1].archived = 1; previous = writes;
    click(&ctx, &state, cards, "Save"); assert(state.error && !state.target_card_version && writes == previous);
    cards[1].archived = 0; destination(&ctx, &state, cards);
    sql(db, "CREATE TRIGGER fail BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'late'); END");
    click(&ctx, &state, cards, "Save"); assert(state.error && state.action == WENA_CHECKLIST_MOVE);
    assert(state.snapshot->checklist_count == 1);
    sql(db, "DROP TRIGGER fail");
    /* Postcommit refresh failure disables writes. Retry only reads the source. */
    fail_reload = 1; click(&ctx, &state, cards, "Save");
    assert(state.needs_refresh && state.error && !state.action);
    previous = writes; previous_reads = reads;
    character(&ctx, &state, cards, 0); assert(writes == previous && reads == previous_reads);
    fail_reload = 0; click(&ctx, &state, cards, "Refresh");
    assert(!state.needs_refresh && !state.error && !state.snapshot->checklist_count &&
        !state.snapshot->item_count && writes == previous);
    wena_checklists_close(&state); assert(sqlite3_close(db) == SQLITE_OK);
    assert(sqlite3_open(argv[4], &db) == SQLITE_OK);
    assert(wena_checklist_mutation_init(&adapter, db, "u", "b"));
    snapshot = wena_checklist_snapshot_create(); assert(snapshot);
    assert(wena_checklist_mutation_load(&adapter, "b", "dst", snapshot));
    assert(snapshot->checklist_count == 1 && snapshot->item_count == 1 &&
        snapshot->items[0].is_finished && snapshot->items[0].position == 19);
    wena_checklist_snapshot_free(snapshot); nk_free(&ctx); assert(sqlite3_close(db) == SQLITE_OK);
    puts("Real Nuklear/SQLite cross-card checklist selection, stale, cancel, rollback, refresh and reopen passed");
    return 0;
}
