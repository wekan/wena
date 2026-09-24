#include "../client/features/labels/panel.h"
#include <nuklear.h>
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static WenaLabelSnapshot store;
static WenaLabelEdit last;
static char last_id[WENA_ID_CAPACITY], last_name[WENA_LABEL_NAME_CAPACITY];
static char last_color[WENA_COLOR_CAPACITY];
static int writes, fail_load, fail_save, fail_after_save;

static int load(void *context, const char *board, const char *card,
    WenaLabelSnapshot *snapshot)
{
    (void)context;
    assert(!strcmp(board, "b"));
    if (fail_load) return 0;
    *snapshot = store;
    if (card) {
        assert(!strcmp(card, "c"));
        strcpy(snapshot->card_id, card);
    } else {
        snapshot->card_id[0] = 0;
        snapshot->card_version = 0;
        memset(snapshot->assigned, 0, sizeof(snapshot->assigned));
    }
    return 1;
}

static int save(void *context, const char *board, const char *card,
    const WenaLabelEdit *edit)
{
    size_t index;
    char id[32];
    (void)context;
    assert(!strcmp(board, "b"));
    assert(!card || !strcmp(card, "c"));
    ++writes;
    last = *edit;
    strcpy(last_id, edit->label_id);
    if (edit->name) strcpy(last_name, edit->name);
    if (edit->color) strcpy(last_color, edit->color);
    if (fail_save || edit->expected_board_version != store.board_version ||
        (card && edit->expected_card_version != store.card_version)) return 0;
    index = 0;
    if (edit->action != WENA_LABEL_CREATE) {
        for (index = 0; index < store.label_count; ++index)
            if (!strcmp(edit->label_id, store.labels[index].id)) break;
        if (index == store.label_count || edit->expected_label_version !=
            store.label_versions[index]) return 0;
    }
    if (edit->action == WENA_LABEL_CREATE) {
        index = store.label_count;
        assert(index < WENA_BOARD_LABEL_CAPACITY);
        sprintf(id, "new%u", (unsigned int)writes);
        assert(wena_label_init(&store.labels[index], id, "b", edit->name,
            edit->color, index ? store.labels[index - 1].position + 1 : 0));
        store.label_versions[index] = 1;
        store.assigned[index] = 0;
        store.assigned_card_counts[index] = 0;
        ++store.label_count;
    } else if (edit->action == WENA_LABEL_EDIT) {
        strcpy(store.labels[index].name, edit->name);
        strcpy(store.labels[index].color, edit->color);
        ++store.label_versions[index];
    } else if (edit->action == WENA_LABEL_ASSIGN || edit->action == WENA_LABEL_UNASSIGN) {
        assert(card);
        store.assigned[index] = edit->action == WENA_LABEL_ASSIGN;
        store.assigned_card_counts[index] = (unsigned long)store.assigned[index];
        ++store.card_version;
    } else if (edit->action == WENA_LABEL_DELETE) {
        if (store.assigned[index]) ++store.card_version;
        for (; index + 1 < store.label_count; ++index) {
            store.labels[index] = store.labels[index + 1];
            store.label_versions[index] = store.label_versions[index + 1];
            store.assigned[index] = store.assigned[index + 1];
            store.assigned_card_counts[index] = store.assigned_card_counts[index + 1];
        }
        --store.label_count;
    }
    ++store.board_version;
    if (fail_after_save) fail_load = 1;
    return 1;
}

static void frame(WenaLabelsState *state, WenaCard *card,
    const char *button, const char *name)
{
    struct nk_context context;
    memset(&context, 0, sizeof(context));
    context.button_to_press = button;
    context.edit_text = name;
    assert(wena_labels_render(&context, state, "b", card, card ? 1 : 0, 800, 600));
    assert(context.begin_count == context.end_count);
}

static void open_create(WenaLabelsState *state, WenaCard *card)
{
    frame(state, card, "Create Label", NULL);
    assert(state->action == WENA_LABEL_CREATE);
}

static void custom_color(WenaLabelsState *state, const char *color)
{
    assert(strlen(color) < sizeof(state->color_input.custom_color));
    strcpy(state->color_input.custom_color, color);
    state->color_input.color_length = (int)strlen(color);
    state->color_input.use_custom_color = 1;
}

static void color_input_tests(void)
{
    WenaColorInput first,second,before;const WenaColorContract *colors;size_t count,i;
    memset(&first,0,sizeof(first));memset(&second,0,sizeof(second));
    assert(wena_color_input_set(&first,"#aBcD01"));
    assert(!strcmp(wena_color_input_value(&first),"#aBcD01"));
    assert(!strcmp(first.custom_color,"#abcd01"));
    assert(wena_color_input_set(&second,"red"));
    before=first;assert(!wena_color_input_set(&first,"#abc"));assert(!memcmp(&before,&first,sizeof(first)));
    assert(!wena_color_input_set(&first,"theme-dark"));assert(!memcmp(&before,&first,sizeof(first)));
    colors=wena_colors(&count);assert(count==25);
    for(i=0;i<count;++i){assert(wena_color_input_set(&first,colors[i].name));
        assert(!strcmp(wena_color_input_value(&first),colors[i].name));}
    assert(!strcmp(wena_color_input_value(&second),"red"));
    assert(wena_color_input_set(&first,""));assert(!strcmp(wena_color_input_value(&first),""));
    first.use_custom_color=1;first.color_length=8;strcpy(first.custom_color,"#1234567");
    assert(!wena_color_input_value(&first));first.color_length=-1;assert(!wena_color_input_value(&first));
    first.color_length=7;strcpy(first.custom_color,"#12zz34");assert(!wena_color_input_value(&first));
    strcpy(first.custom_color,"#aBcD01");assert(!strcmp(wena_color_input_value(&first),"#aBcD01"));
    assert(wena_color_input_set(&first,first.custom_color));assert(!first.use_custom_color);
    assert(wena_color_input_set(&first,first.color));
    first.use_custom_color=0;memset(first.color,'x',sizeof(first.color));assert(!wena_color_input_value(&first));
    assert(!wena_color_input_value(NULL));assert(!wena_color_input_set(NULL,"red"));
}
int main(void)
{
    WenaLabelsState state;
    WenaCard card, duplicates[2];
    WenaLabel temporary;
    struct nk_context context;
    char long_name[150];
    int before;
    unsigned long revision;
    memset(&store, 0, sizeof(store));
    strcpy(store.board_id, "b");
    strcpy(store.card_id, "c");
    store.board_version = 1;
    store.card_version = 2;
    store.label_count = 2;
    store.label_versions[0] = 3;
    store.label_versions[1] = 4;
    color_input_tests();
    assert(wena_label_init(&store.labels[0], "label1", "b", "Same", "white", 0));
    assert(wena_label_init(&store.labels[1], "label2", "b", "Same", "green", 1));
    assert(wena_card_init(&card, "c", "b", "s", "l", "Card", 0, 0));
    wena_labels_init(&state, load, save, NULL);
    assert(wena_labels_open(&state, "b", &card));
    open_create(&state, &card);
    assert(strcmp(state.color_input.color, "white") && strcmp(state.color_input.color, "green"));
    frame(&state, &card, "Cancel", "Discard");
    assert(!state.action && !writes);
    open_create(&state, &card);
    frame(&state, &card, "Create", "");
    assert(writes == 1 && !state.action && !last_name[0]);
    open_create(&state, &card);
    memset(long_name, 'x', sizeof(long_name));
    long_name[sizeof(long_name) - 1] = 0;
    frame(&state, &card, "Create", long_name);
    assert(state.error && state.name_length == 132 && writes == 1);
    frame(&state, &card, "Create", "bad\001input");
    assert(state.error && writes == 1);
    frame(&state, &card, "Create", "bad\ninput");
    assert(state.error && writes == 1);
    custom_color(&state, "#1234567");
    frame(&state, &card, "Create", "Valid");
    assert(state.error && writes == 1);
    custom_color(&state, "#12zz34");
    frame(&state, &card, "Create", NULL);
    assert(state.error && writes == 1);
    custom_color(&state, "#aBcDeF");
    frame(&state, &card, "Create", " \302\240\357\273\277Trim\343\200\200 ");
    assert(writes == 2 && !state.action && !strcmp(last_name, "Trim") &&
        !strcmp(last_color, "#aBcDeF"));
    frame(&state, &card, "Change Label", NULL);
    assert(state.action == WENA_LABEL_EDIT && !strcmp(state.label_id, "label1"));
    revision = store.label_versions[0];
    frame(&state, &card, "Save", "Edited");
    assert(!state.action && !strcmp(last_id, "label1") &&
        last.expected_label_version == revision && !strcmp(store.labels[0].name, "Edited"));
    frame(&state, &card, "Edited", NULL);
    assert(!state.action && state.snapshot->assigned[0] && last.action == WENA_LABEL_ASSIGN);
    frame(&state, &card, "Edited", NULL);
    assert(!state.snapshot->assigned[0] && last.action == WENA_LABEL_UNASSIGN);
    fail_save = 1;
    frame(&state, &card, "Edited", NULL);
    assert(state.error && !state.action && !state.snapshot->assigned[0]);
    fail_save = 0;
    frame(&state, &card, "Change Label", NULL);
    before = writes;
    ++store.board_version;
    frame(&state, &card, "Save", "Retained draft");
    assert(state.error && state.action == WENA_LABEL_EDIT && writes == before + 1 &&
        !strcmp(state.name, "Retained draft") && strcmp(store.labels[0].name, state.name));
    frame(&state, &card, "Cancel", NULL);
    wena_labels_close(&state);
    assert(wena_labels_open(&state, "b", &card));
    frame(&state, &card, "Change Label", NULL);
    frame(&state, &card, "Delete", "Unsaved\001rename");
    assert(state.action == WENA_LABEL_DELETE && !strcmp(state.name, "Edited") &&
        !strcmp(state.color_input.color, "white"));
    before = writes;
    frame(&state, &card, "Cancel", NULL);
    assert(writes == before && store.label_count == 4);
    frame(&state, &card, "Change Label", NULL);
    frame(&state, &card, "Delete", NULL);
    fail_save = 1;
    frame(&state, &card, "Delete", NULL);
    assert(state.error && state.action == WENA_LABEL_DELETE && store.label_count == 4);
    fail_save = 0;
    fail_after_save = 1;
    frame(&state, &card, "Delete", NULL);
    assert(!state.action && state.needs_refresh && state.error && store.label_count == 3);
    before = writes;
    frame(&state, &card, "Delete", NULL);
    frame(&state, &card, "Save", NULL);
    frame(&state, &card, "Refresh", NULL);
    assert(writes == before && state.needs_refresh);
    fail_load = 0;
    fail_after_save = 0;
    frame(&state, &card, "Refresh", NULL);
    assert(writes == before && !state.needs_refresh && !state.error);
    wena_labels_close(&state);
    /* Two equal names stay distinguishable by stored identity, never text. */
    temporary = store.labels[0];
    store.labels[0] = store.labels[1];
    store.labels[1] = temporary;
    store.labels[0].position = 0;
    store.labels[1].position = 1;
    strcpy(store.labels[0].name, "Same");
    strcpy(store.labels[1].name, "Same");
    assert(wena_labels_open(&state, "b", &card));
    frame(&state, &card, "Change Label", NULL);
    assert(!strcmp(state.label_id, store.labels[0].id));
    frame(&state, &card, "Save", "Distinct");
    assert(!strcmp(last_id, store.labels[0].id) && !strcmp(store.labels[1].name, "Same"));
    frame(&state, &card, "Close details", NULL);
    assert(!state.visible && !state.snapshot);
    wena_labels_close(&state);
    /* Board mode shares the editor, but never sends a selected-card mutation. */
    assert(wena_labels_open(&state, "b", NULL));
    before = writes;
    frame(&state, NULL, "Distinct", NULL);
    assert(writes == before && !state.action);
    open_create(&state, NULL);
    frame(&state, NULL, "Create", "Board label");
    assert(writes == before + 1 && last.expected_card_version == 0);
    wena_labels_close(&state);
    wena_labels_init(&state, load, NULL, NULL);
    assert(wena_labels_open(&state, "b", &card));
    before = writes;
    frame(&state, &card, "Create Label", NULL);
    frame(&state, &card, "Change Label", NULL);
    frame(&state, &card, "Distinct", NULL);
    assert(!state.action && writes == before);
    memset(&context, 0, sizeof(context));
    assert(!wena_labels_render(&context, &state, "other", &card, 1, 800, 600));
    assert(!state.visible && !state.snapshot);
    assert(!wena_labels_open(&state, "other", &card));
    card.archived = 1;
    assert(!wena_labels_open(&state, "b", &card));
    card.archived = 0;
    assert(wena_labels_open(&state, "b", &card));
    duplicates[0] = card;
    duplicates[1] = card;
    assert(!wena_labels_render(&context, &state, "b", duplicates, 2, 800, 600));
    store.board_version = 0;
    assert(!wena_labels_open(&state, "b", &card));
    store.board_version = 8;
    store.label_versions[0] = (unsigned long)LONG_MAX;
    assert(!wena_labels_open(&state, "b", &card));
    store.label_versions[0] = 1;
    store.label_count = WENA_BOARD_LABEL_CAPACITY + 1;
    assert(!wena_labels_open(&state, "b", &card));
    store.label_count = 1;
    strcpy(store.labels[0].board_id, "other");
    assert(!wena_labels_open(&state, "b", &card));
    wena_labels_close(&state);
    puts("Labels panel validation, drafts, identity, scope and refresh guards passed");
    return 0;
}
