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
static int writes, fail_save;
static int load(void *context,const char *board,const char *card,WenaChecklistSnapshot *out)
{
    (void)context;
    assert(!strcmp(board,"b") && !strcmp(card,"c")); *out=store; return 1;
}
static int save(void *context,const char *board,const char *card,const WenaChecklistEdit *edit)
{
    WenaChecklist list;
    WenaChecklistItem item;
    unsigned long version;
    size_t index,selected,target,count;
    (void)context;
    assert(!strcmp(board,"b") && !strcmp(card,"c"));
    assert(edit->expected_card_version==store.card_version);
    assert(!edit->title);
    ++writes; if(fail_save) return 0;
    target=(size_t)edit->target_position;
    if(edit->action==WENA_CHECKLIST_REORDER) {
        count=store.checklist_count;
        for(selected=0;selected<count;++selected)
            if(!strcmp(store.checklists[selected].id,edit->checklist_id)) break;
        assert(selected<count && target<count);
        assert(edit->expected_checklist_version==store.checklist_versions[selected]);
        if(selected==target) return 1;
        list=store.checklists[selected]; version=store.checklist_versions[selected];
        if(selected<target) {
            memmove(store.checklists+selected,store.checklists+selected+1,(target-selected)*sizeof(list));
            memmove(store.checklist_versions+selected,store.checklist_versions+selected+1,(target-selected)*sizeof(version));
        } else {
            memmove(store.checklists+target+1,store.checklists+target,(selected-target)*sizeof(list));
            memmove(store.checklist_versions+target+1,store.checklist_versions+target,(selected-target)*sizeof(version));
        }
        store.checklists[target]=list; store.checklist_versions[target]=version;
        for(index=0;index<count;++index) {
            if(store.checklists[index].position!=index) ++store.checklist_versions[index];
            store.checklists[index].position=(unsigned long)index;
        }
    } else {
        assert(edit->action==WENA_CHECKLIST_REORDER_ITEM);
        assert(store.item_count==2 && target<2 && !strcmp(edit->checklist_id,"cl0"));
        selected=!strcmp(store.items[0].id,edit->item_id)?0:1;
        assert(!strcmp(store.items[selected].id,edit->item_id));
        assert(edit->expected_item_version==store.item_versions[selected]);
        if(selected==target) return 1;
        item=store.items[0]; store.items[0]=store.items[1]; store.items[1]=item;
        version=store.item_versions[0]; store.item_versions[0]=store.item_versions[1]; store.item_versions[1]=version;
        for(index=0;index<2;++index) {
            if(store.items[index].position!=index) ++store.item_versions[index];
            store.items[index].position=(unsigned long)index;
        }
        for(index=0;index<store.checklist_count;++index)
            if(!strcmp(store.checklists[index].id,"cl0")) {
                assert(edit->expected_checklist_version==store.checklist_versions[index]);
                ++store.checklist_versions[index];
            }
    }
    ++store.card_version; return 1;
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

static void open_order(struct nk_context *ctx, WenaChecklistsState *state,
    WenaCard *card, int items)
{
    character(ctx, state, card, 0);
    click(ctx, state, card, items ? "Edit" : "Rename");
    character(ctx, state, card, 0);
    click(ctx, state, card, wena_ui_text(WENA_UI_TEXT_MOVE_SELECTION));
    character(ctx, state, card, 0);
    assert(state->action == (items ? WENA_CHECKLIST_REORDER_ITEM : WENA_CHECKLIST_REORDER));
}

int main(void)
{
    struct nk_context ctx;
    struct nk_user_font font;
    WenaChecklistsState state;
    WenaCard card;
    int previous;
    memset(&store, 0, sizeof(store));
    strcpy(store.board_id, "b"); strcpy(store.card_id, "c"); store.card_version = 1;
    store.checklist_count = 3; store.item_count = 2;
    store.checklist_versions[0] = store.checklist_versions[1] = store.checklist_versions[2] = 1;
    store.item_versions[0] = store.item_versions[1] = 1;
    assert(wena_checklist_init(&store.checklists[0], "cl0", "b", "c", "First checklist", 0));
    assert(wena_checklist_init(&store.checklists[1], "cl1", "b", "c", "Second checklist", 5));
    assert(wena_checklist_init(&store.checklists[2], "cl2", "b", "c", "Third checklist", 9));
    assert(wena_checklist_item_init(&store.items[0], "it0", "b", "c", "cl0", "First item", 0, 0));
    assert(wena_checklist_item_init(&store.items[1], "it1", "b", "c", "cl0", "Second item", 8, 1));
    memset(&font, 0, sizeof(font)); font.height = 13; font.width = text_width;
    assert(nk_init_default(&ctx, &font));
    assert(wena_card_init(&card, "c", "b", "s", "l", "Card", 0, 0));
    wena_checklists_init(&state, load, save, NULL);
    assert(wena_checklists_open(&state, &card)); render(&ctx, &state, &card);
    open_order(&ctx, &state, &card, 0);
    assert(state.order_position == 0 && state.order_count == 3);
    key(&ctx, &state, &card, NK_KEY_ENTER, 1);
    key(&ctx, &state, &card, NK_KEY_ENTER, 0); assert(!writes && state.action);
    click(&ctx, &state, &card, "1"); character(&ctx, &state, &card, 0);
    click(&ctx, &state, &card, "3"); assert(state.order_position == 2 && !writes);
    click(&ctx, &state, &card, "Cancel"); assert(!state.action && !writes);
    open_order(&ctx, &state, &card, 0);
    click(&ctx, &state, &card, "1"); character(&ctx, &state, &card, 0);
    click(&ctx, &state, &card, "3"); click(&ctx, &state, &card, "Save");
    assert(writes == 1 && !state.action && !strcmp(store.checklists[2].id, "cl0"));
    assert(state.snapshot->checklists[2].position == 2);
    /* Item order includes completed items and keeps its checklist scope even
     * after the checklist's own display ordinal changed. */
    open_order(&ctx, &state, &card, 1);
    assert(state.order_position == 0 && state.order_count == 2 && !strcmp(state.item_id, "it0"));
    click(&ctx, &state, &card, "1"); character(&ctx, &state, &card, 0);
    key(&ctx, &state, &card, NK_KEY_ENTER, 1);
    key(&ctx, &state, &card, NK_KEY_ENTER, 0); assert(writes == 1);
    click(&ctx, &state, &card, "2"); click(&ctx, &state, &card, "Cancel");
    assert(writes == 1 && !state.action && !strcmp(store.items[0].id, "it0"));
    open_order(&ctx, &state, &card, 1);
    click(&ctx, &state, &card, "1"); character(&ctx, &state, &card, 0);
    click(&ctx, &state, &card, "2"); fail_save = 1;
    click(&ctx, &state, &card, "Save");
    assert(state.error && state.action == WENA_CHECKLIST_REORDER_ITEM && state.order_position == 1);
    assert(!strcmp(store.items[0].id, "it0")); previous = writes; fail_save = 0;
    click(&ctx, &state, &card, "Save");
    assert(writes == previous + 1 && !state.action && !state.error &&
        !strcmp(state.snapshot->items[1].id, "it0") && state.snapshot->items[0].is_finished);
    previous = writes;
    open_order(&ctx, &state, &card, 1);
    click(&ctx, &state, &card, "1"); character(&ctx, &state, &card, 0);
    key(&ctx, &state, &card, NK_KEY_TEXT_RESET_MODE, 1);
    assert(!state.visible && !state.snapshot && writes == previous);
    key(&ctx, &state, &card, NK_KEY_TEXT_RESET_MODE, 0);
    /* Existing rename draft is discarded only by explicit Move selection;
     * the dialog always names the actual stored object. */
    assert(wena_checklists_open(&state, &card)); character(&ctx, &state, &card, 0);
    click(&ctx, &state, &card, "Rename"); character(&ctx, &state, &card, 0);
    strcpy(state.input, "Unsaved draft"); state.length = (int)strlen(state.input);
    click(&ctx, &state, &card, wena_ui_text(WENA_UI_TEXT_MOVE_SELECTION));
    assert(state.action == WENA_CHECKLIST_REORDER && !strcmp(state.input, store.checklists[0].title));
    click(&ctx, &state, &card, "Cancel"); assert(writes == previous);
    wena_checklists_close(&state); nk_free(&ctx);
    puts("Real Nuklear checklist order selection,Save,Cancel,Enter,Escape and retained failure passed");
    return 0;
}
