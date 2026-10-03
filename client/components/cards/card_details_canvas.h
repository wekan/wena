#ifndef WENA_CARD_DETAILS_CANVAS_H
#define WENA_CARD_DETAILS_CANVAS_H

#include "../../../models/card.h"

struct nk_context;

#define WENA_CARD_DETAILS_NO_ACTION 0u
#define WENA_CARD_DETAILS_EDIT_TITLE 1u
#define WENA_CARD_DETAILS_ARCHIVE 2u
#define WENA_CARD_DETAILS_CLOSE 4u
#define WENA_CARD_DETAILS_MOVE 8u
#define WENA_CARD_DETAILS_DESCRIPTION 16u
#define WENA_CARD_DETAILS_CHECKLISTS 32u
#define WENA_CARD_DETAILS_LABELS 64u
#define WENA_CARD_DETAILS_OPEN_MENU 128u   /* Card Actions */

/* WeKan's card details sections Wena has data for, in WeKan's order. */
typedef enum WenaCardDetailsSection {
    WENA_CARD_DETAILS_SECTION_LABELS,
    WENA_CARD_DETAILS_SECTION_DESCRIPTION,
    WENA_CARD_DETAILS_SECTION_CHECKLISTS,
    WENA_CARD_DETAILS_SECTION_COUNT
} WenaCardDetailsSection;

/* A section's contents (label chips, the description, the checklists),
 * drawn by the host from what it has loaded; returns WENA_CARD_DETAILS_*
 * actions, such as LABELS when a chip is clicked. */
typedef unsigned int (*WenaCardDetailsSectionBody)(struct nk_context *context, void *user,
    const WenaCard *card, WenaCardDetailsSection section);

typedef struct WenaCardDetailsView {
    int collapsed;   /* only the header, as WeKan's caret folds the card */
    int maximized;   /* the whole window wide */
    int closed[WENA_CARD_DETAILS_SECTION_COUNT];
    WenaCardDetailsSectionBody body;
    void *body_context;
} WenaCardDetailsView;

/* WeKan's card details: the header (Collapse, the title - click to edit -,
 * Card Actions, Maximize, Close Card) and the sections, each with its caret,
 * icon and title. Toggles change `view`; the rest are returned as actions. */
unsigned int wena_card_details_canvas_render(struct nk_context *context,
                                             const WenaCard *card,
                                             WenaCardDetailsView *view);

#endif
