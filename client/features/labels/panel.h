#ifndef WENA_LABELS_PANEL_H
#define WENA_LABELS_PANEL_H

#include "store.h"
#include "../card_details.h"
#include "../../components/forms/color_input.h"

typedef int (*WenaLabelsLoad)(void *context, const char *board_id,
    const char *card_id, WenaLabelSnapshot *snapshot);
typedef int (*WenaLabelsSave)(void *context, const char *board_id,
    const char *card_id, const WenaLabelEdit *edit);

typedef struct WenaLabelsState {
    int visible;
    int error;
    int needs_refresh;
    WenaId board_id;
    WenaId card_id;
    WenaId label_id;
    WenaLabelAction action;
    unsigned long board_version;
    unsigned long label_version;
    unsigned long card_version;
    int name_length;
    WenaColorInput color_input;
    unsigned long affected_cards;
    /* Four overflow bytes admit one complete UTF-8 scalar beyond a full valid
     * name, so the panel can reject it instead of silently saving the prefix. */
    char name[WENA_LABEL_NAME_CAPACITY + 4];
    WenaLabelSnapshot *snapshot;
    WenaLabelsLoad load;
    WenaLabelsSave save;
    void *context;
} WenaLabelsState;

/* The panel owns its snapshot; close before reinitializing an open state.
 * NULL save enables inspection only. An omitted card opens board label editing.
 * A committed save followed by a failed reload requires Refresh, never replay. */
void wena_labels_init(WenaLabelsState *state, WenaLabelsLoad load,
    WenaLabelsSave save, void *context);
void wena_labels_close(WenaLabelsState *state);
int wena_labels_open(WenaLabelsState *state, const char *board_id,
    const WenaCard *card);
int wena_labels_render(struct nk_context *context, WenaLabelsState *state,
    const char *board_id, const WenaCard *cards, size_t card_count,
    float width, float height);

#endif
