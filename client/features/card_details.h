#ifndef WENA_CARD_DETAILS_FEATURE_H
#define WENA_CARD_DETAILS_FEATURE_H

#include "../../models/card.h"
#include "../components/cards/card_details_canvas.h"
#include "../components/forms/input_limits.h"

#include <stddef.h>

struct nk_context;

#define WENA_CARD_DETAILS_TITLE_CAPACITY 129

typedef int (*WenaCardDetailsLoadTitle)(void *context,
    const char *board_id, const char *card_id, char *title,
    size_t capacity, unsigned long *version);
typedef int (*WenaCardDetailsSaveTitle)(void *context,
    const char *board_id, const char *card_id,
    unsigned long expected_version, const char *title);

typedef int (*WenaCardDetailsArchive)(void *context,
    const char *board_id, const char *card_id, unsigned long expected_version);

typedef struct WenaCardDetailsInteraction {
    unsigned int actions;
    WenaId card_id;
} WenaCardDetailsInteraction;

typedef struct WenaCardDetailsState {
    int visible;
    WenaId card_id;
    WenaId board_id;
    int editing_title;
    int title_error;
    int title_length;
    char title_input[WENA_NATIVE_EDIT_CAPACITY(WENA_CARD_DETAILS_TITLE_CAPACITY)];
    unsigned long title_version;
    WenaCardDetailsLoadTitle load_title;
    WenaCardDetailsSaveTitle save_title;
    WenaCardDetailsArchive archive_card;
    void *title_context;
    WenaCardDetailsInteraction interaction;
} WenaCardDetailsState;

void wena_card_details_set_title_adapter(WenaCardDetailsState *state,
    WenaCardDetailsLoadTitle load, WenaCardDetailsSaveTitle save, void *context);
/* Shares the authenticated title adapter context and loaded row version. */
void wena_card_details_set_archive_adapter(WenaCardDetailsState *state,
    WenaCardDetailsArchive archive);
#define WENA_TITLE_INPUT_COMMIT 1u
#define WENA_TITLE_INPUT_CANCEL 2u
/* Call while the title panel is the current Nuklear window. Only its focused
 * edit commit or focused-window Escape becomes an action; cancellation wins. */
unsigned int wena_title_input_keys(struct nk_context *context,
                                    unsigned int edit_result);
int wena_card_details_title_valid(const char *title, size_t length);
void wena_card_details_init(WenaCardDetailsState *state);
int wena_card_details_open(WenaCardDetailsState *state, const WenaCard *card);
void wena_card_details_close(WenaCardDetailsState *state);
int wena_card_details_render(struct nk_context *context,
                             WenaCardDetailsState *state,
                             const WenaCard *cards, size_t card_count,
                             float width, float height);

#endif
