#ifndef WENA_CARD_DESCRIPTION_H
#define WENA_CARD_DESCRIPTION_H
#include "card_details.h"

typedef int (*WenaCardDescriptionLoad)(void *context, const char *board_id,
    const char *card_id, char *description, size_t capacity, unsigned long *version);
typedef int (*WenaCardDescriptionSave)(void *context, const char *board_id,
    const char *card_id, unsigned long expected_version, const char *description);

typedef struct WenaCardDescriptionState {
    int visible;
    int error;
    int length;
    WenaId board_id;
    WenaId card_id;
    unsigned long version;
    /* One extra byte detects overlong edits; Nuklear also reserves a NUL. */
    char input[WENA_DESCRIPTION_CAPACITY + 1];
    WenaCardDescriptionLoad load;
    WenaCardDescriptionSave save;
    void *context;
} WenaCardDescriptionState;

void wena_card_description_init(WenaCardDescriptionState *state,
    WenaCardDescriptionLoad load, WenaCardDescriptionSave save, void *context);
void wena_card_description_close(WenaCardDescriptionState *state);
/* A NULL save callback opens a read-only view. Empty descriptions are valid. */
int wena_card_description_open(WenaCardDescriptionState *state, const WenaCard *card);
int wena_card_description_render(struct nk_context *context,
    WenaCardDescriptionState *state, const WenaCard *cards, size_t card_count,
    float width, float height);
#endif
