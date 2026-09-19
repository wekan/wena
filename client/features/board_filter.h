#ifndef WENA_BOARD_FILTER_H
#define WENA_BOARD_FILTER_H
#include "../../models/card.h"
#include "../components/forms/input_limits.h"
struct nk_context;
#define WENA_BOARD_FILTER_CAPACITY 129
/* Session-only literal title substring: ASCII case-insensitive, other UTF-8
 * bytes exact. No regex, accent folding, labels, members or stored mutation. */
typedef struct WenaBoardFilterState {
    WenaId board_id;
    char query[WENA_BOARD_FILTER_CAPACITY];
    char input[WENA_NATIVE_EDIT_CAPACITY(WENA_BOARD_FILTER_CAPACITY)];
    int length;
    int error;
} WenaBoardFilterState;
void wena_board_filter_init(WenaBoardFilterState *state);
/* Changing board discards both applied query and draft. Invalid IDs fail closed. */
int wena_board_filter_sync(WenaBoardFilterState *state, const char *board_id);
int wena_board_filter_apply(WenaBoardFilterState *state);
void wena_board_filter_clear(WenaBoardFilterState *state);
void wena_board_filter_cancel(WenaBoardFilterState *state);
/* Suitable directly as WenaBoardLayout.card_visible callback. */
int wena_board_filter_matches(void *context, const WenaCard *card);
/* Owns one row. Returns nonzero only when the applied query changes; caller
 * closes other card panels on that event, without clearing model selection IDs. */
int wena_board_filter_render(struct nk_context *context, WenaBoardFilterState *state);
#endif
