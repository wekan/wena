#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_IMPLEMENTATION
#include <nuklear.h>
#include "../client/features/checklists.h"
#include "../imports/ui/page_contract.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static WenaChecklistSnapshot store;
static int writes, fail_save, fail_load;
static int load(void *ctx,const char *board,const char *card,WenaChecklistSnapshot *out)
{
    (void)ctx;
    assert(!strcmp(board,"b") && !strcmp(card,"c"));
    if(fail_load) return 0;
    *out=store; return 1;
}
static int save(void *ctx,const char *board,const char *card,const WenaChecklistEdit *edit)
{
    char titles[WENA_CHECKLIST_BATCH_MAX_ITEMS][WENA_CHECKLIST_TITLE_CAPACITY];
    char id[65];
    size_t count,index;
    (void)ctx;
    assert(!strcmp(board,"b") && !strcmp(card,"c"));
    assert(edit->action==WENA_CHECKLIST_ADD_ITEMS);
    assert(edit->expected_card_version==store.card_version);
    assert(edit->expected_checklist_version==store.checklist_versions[0]);
    assert(!strcmp(edit->checklist_id,"cl"));
    assert(!edit->title);
    assert(wena_checklist_item_batch_parse(edit->batch_text,edit->batch_length,titles,&count));
    ++writes;
    if(fail_save) return 0;
    for(index=0;index<count;++index) {
        sprintf(id,"item-%lu",(unsigned long)store.item_count);
        assert(wena_checklist_item_init(&store.items[store.item_count],id,"b","c","cl",titles[index],(unsigned long)store.item_count,0));
        store.item_versions[store.item_count++]=1;
    }
    ++store.card_version;
    ++store.checklist_versions[0];
    return 1;
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
    if(state->visible) assert(wena_checklists_render(ctx,state,card,1,1000,800));
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

static void open_batch(struct nk_context *ctx, WenaChecklistsState *state,
    WenaCard *card)
{
    character(ctx, state, card, 0);
    click(ctx, state, card, "Add an item to checklist");
    character(ctx, state, card, 0);
    assert(state->action == WENA_CHECKLIST_ADD_ITEM);
    click(ctx, state, card, wena_ui_text(WENA_UI_TEXT_CHECKLIST_SPLIT_LINES));
    character(ctx, state, card, 0);
    assert(state->action == WENA_CHECKLIST_ADD_ITEMS);
    click_at(ctx, state, card, nk_vec2(500, 150));
}

int main(void)
{
    struct nk_context ctx;
    struct nk_user_font font;
    WenaChecklistsState state;
    WenaCard card;
    int index, item, previous;
    memset(&store, 0, sizeof(store));
    strcpy(store.board_id, "b"); strcpy(store.card_id, "c"); store.card_version = 1;
    store.checklist_count = 1; store.checklist_versions[0] = 1;
    assert(wena_checklist_init(&store.checklists[0], "cl", "b", "c", "Checklist", 0));
    memset(&font, 0, sizeof(font)); font.height = 13; font.width = text_width;
    assert(nk_init_default(&ctx, &font));
    assert(wena_card_init(&card, "c", "b", "s", "l", "Card", 0, 0));
    wena_checklists_init(&state, load, save, NULL);
    assert(wena_checklists_open(&state, &card)); render(&ctx, &state, &card);
    open_batch(&ctx, &state, &card);
    character(&ctx, &state, &card, 'A');
    key(&ctx, &state, &card, NK_KEY_ENTER, 1);
    assert(writes == 0 && state.action == WENA_CHECKLIST_ADD_ITEMS && state.length == 2);
    key(&ctx, &state, &card, NK_KEY_ENTER, 0);
    character(&ctx, &state, &card, 'B'); assert(!memcmp(state.input, "A\nB", 3));
    /* Reverting to single-line mode must retain the complete multiline draft. */
    click(&ctx, &state, &card, wena_ui_text(WENA_UI_TEXT_CHECKLIST_SPLIT_LINES));
    assert(state.action == WENA_CHECKLIST_ADD_ITEMS && state.length == 3 && state.error);
    click(&ctx, &state, &card, "Save");
    assert(writes == 1 && !state.action && !state.error && state.snapshot->item_count == 2);
    assert(!strcmp(state.snapshot->items[0].title, "A") &&
        !strcmp(state.snapshot->items[1].title, "B"));
    assert(store.card_version == 2 && store.checklist_versions[0] == 2);
    /* Cancel and focused Escape abandon the draft without calling save. */
    open_batch(&ctx, &state, &card); character(&ctx, &state, &card, 'X');
    click(&ctx, &state, &card, "Cancel"); assert(writes == 1 && !state.action);
    open_batch(&ctx, &state, &card); character(&ctx, &state, &card, 'Y');
    key(&ctx, &state, &card, NK_KEY_TEXT_RESET_MODE, 1);
    assert(!state.visible && !state.snapshot && writes == 1);
    key(&ctx, &state, &card, NK_KEY_TEXT_RESET_MODE, 0);
    assert(wena_checklists_open(&state, &card));
    open_batch(&ctx, &state, &card);
    /* Ninth nonblank line is rejected as a whole. */
    for (index = 0; index < 9; ++index) {
        character(&ctx, &state, &card, 'x');
        if (index < 8) {
            key(&ctx, &state, &card, NK_KEY_ENTER, 1);
            key(&ctx, &state, &card, NK_KEY_ENTER, 0);
        }
    }
    click(&ctx, &state, &card, "Save");
    assert(state.error && state.action == WENA_CHECKLIST_ADD_ITEMS && writes == 1);
    click(&ctx, &state, &card, "Cancel");
    open_batch(&ctx, &state, &card);
    /* Retain a full overflow scalar, including a four-byte character beyond
     * an otherwise valid maximum-size draft. */
    for (item = 0; item < 8; ++item) {
        for (index = 0; index < 128; ++index)
            character(&ctx, &state, &card, 'z');
        if (item < 7) {
            key(&ctx, &state, &card, NK_KEY_ENTER, 1);
            key(&ctx, &state, &card, NK_KEY_ENTER, 0);
        }
    }
    assert(state.length == (int)WENA_CHECKLIST_BATCH_MAX_BYTES);
    character(&ctx, &state, &card, 128640UL);
    assert(state.length == (int)WENA_CHECKLIST_BATCH_MAX_BYTES + 4);
    click(&ctx, &state, &card, "Save"); assert(state.error && writes == 1);
    click(&ctx, &state, &card, "Cancel");
    open_batch(&ctx, &state, &card);
    character(&ctx, &state, &card, 'R'); fail_save = 1;
    click(&ctx, &state, &card, "Save");
    assert(state.error && state.length == 1 && state.action == WENA_CHECKLIST_ADD_ITEMS);
    previous = writes; fail_save = 0; fail_load = 1;
    click(&ctx, &state, &card, "Save");
    assert(writes == previous + 1 && state.needs_refresh && !state.action);
    key(&ctx, &state, &card, NK_KEY_ENTER, 1);
    key(&ctx, &state, &card, NK_KEY_ENTER, 0);
    assert(writes == previous + 1);
    fail_load = 0; click(&ctx, &state, &card, "Refresh");
    assert(!state.needs_refresh && !state.error && state.snapshot->item_count == 3);
    wena_checklists_close(&state); nk_free(&ctx);
    puts("Real Nuklear checklist batch newline,Cancel,Escape,bounds,rollback draft and refresh passed");
    return 0;
}
