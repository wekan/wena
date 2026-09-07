#include "card_archives.h"
#include "../../imports/ui/page_contract.h"
#include <nuklear.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

typedef struct ArchiveOptions {
    const WenaBoardLayout *layout;
    char label[WENA_TITLE_CAPACITY + WENA_ID_CAPACITY + 8];
} ArchiveOptions;

static int layout_valid(const WenaBoardLayout *layout)
{
    return layout != NULL && layout->board != NULL && !layout->board->archived &&
        (layout->card_count == 0 || layout->cards != NULL) &&
        layout->card_count <= (size_t)(INT_MAX / 64);
}

static const WenaCard *option_card(const WenaBoardLayout *layout, int selected)
{
    size_t i;
    int index;
    index = 0;
    for (i = 0; i < layout->card_count; ++i) {
        const WenaCard *card;
        card = &layout->cards[i];
        if (card->archived && !strcmp(card->board_id, layout->board->id)) {
            if (index++ == selected) return card;
        }
    }
    return NULL;
}

static void option_label(void *data, int index, const char **label)
{
    ArchiveOptions *options;
    const WenaCard *card;
    options = (ArchiveOptions *)data;
    card = option_card(options->layout, index);
    if (card == NULL) strcpy(options->label, "[?]");
    else sprintf(options->label, "%s [%s]", card->title, card->id);
    *label = options->label;
}

static void select_card(WenaCardArchivesState *state, const WenaCard *card)
{
    char title[WENA_TITLE_CAPACITY];
    state->version = 0;
    state->error = 0;
    state->card_id[0] = '\0';
    if (card == NULL) return;
    if (!wena_model_set_required(state->card_id, sizeof(state->card_id), card->id) ||
        state->load == NULL || !state->load(state->context, state->board_id,
            state->card_id, title, sizeof(title), &state->version) || state->version == 0) {
        state->version = 0;
        state->error = 1;
    }
}

void wena_card_archives_init(WenaCardArchivesState *state,
    WenaCardDetailsLoadTitle load, WenaCardArchivesRestore restore, void *context)
{
    if (state == NULL) return;
    memset(state, 0, sizeof(*state));
    state->load = load; state->restore = restore; state->context = context;
}

void wena_card_archives_close(WenaCardArchivesState *state)
{
    if (state == NULL) return;
    state->visible = 0; state->error = 0; state->version = 0;
    state->card_id[0] = '\0'; state->board_id[0] = '\0';
}

int wena_card_archives_open(WenaCardArchivesState *state,
    const WenaBoardLayout *layout)
{
    if (state == NULL) return 0;
    wena_card_archives_close(state);
    if (!layout_valid(layout) || !wena_model_set_required(state->board_id,
        sizeof(state->board_id), layout->board->id)) return 0;
    state->visible = 1;
    select_card(state, option_card(layout, 0));
    return 1;
}

int wena_card_archives_render(struct nk_context *context,
    WenaCardArchivesState *state, const WenaBoardLayout *layout,
    float width, float height)
{
    ArchiveOptions options;
    const WenaCard *card;
    int count, selected, choice, close_requested;
    if (state == NULL || !state->visible) return 0;
    if (!layout_valid(layout) || strcmp(state->board_id, layout->board->id)) {
        wena_card_archives_close(state); return 0;
    }
    if (context == NULL || width <= 0 || height <= 0) return 0;
    count = 0; selected = -1;
    while ((card = option_card(layout, count)) != NULL) {
        if (!strcmp(card->id, state->card_id)) selected = count;
        ++count;
    }
    if (selected < 0) {
        select_card(state, option_card(layout, 0));
        selected = count != 0 ? 0 : -1;
    }
    close_requested = 0;
    if (nk_begin(context, "Archives", nk_rect(width * 0.5f, 0,
        width * 0.5f, height), NK_WINDOW_BORDER)) {
        nk_layout_row_dynamic(context, 28, 1);
        nk_label(context, wena_ui_text(WENA_UI_TEXT_ARCHIVES), NK_TEXT_LEFT);
        if (count != 0) {
            options.layout = layout;
            nk_layout_row_dynamic(context, 28, 1);
            choice = nk_combo_callback(context, option_label, &options, selected,
                count, 24, nk_vec2(280, 220));
            if (choice != selected && choice >= 0 && choice < count)
                select_card(state, option_card(layout, choice));
            nk_layout_row_dynamic(context, 28, 1);
            if (nk_button_label(context, wena_ui_control_text(WENA_UI_RESTORE_CARD))) {
                if (state->version != 0 && state->restore != NULL &&
                    state->restore(state->context, state->board_id,
                                   state->card_id, state->version)) {
                    select_card(state, option_card(layout, 0));
                } else state->error = 1;
            }
        } else {
            nk_layout_row_dynamic(context, 28, 1);
            nk_label(context, wena_ui_text(WENA_UI_TEXT_NO_ARCHIVED_CARDS), NK_TEXT_LEFT);
        }
        nk_layout_row_dynamic(context, 28, 2);
        if (nk_button_label(context, wena_ui_control_text(WENA_UI_CANCEL))) close_requested = 1;
        if (nk_button_label(context, wena_ui_control_text(WENA_UI_CLOSE))) close_requested = 1;
        if (state->error) nk_label(context, "[!]", NK_TEXT_LEFT);
    }
    nk_end(context);
    if (close_requested) wena_card_archives_close(state);
    return 1;
}
