#ifndef WENA_CARD_SECTION_CONTROL_H
#define WENA_CARD_SECTION_CONTROL_H
#include "../../../models/card_section.h"
struct nk_context;
typedef struct WenaCardSectionControl {
    const WenaCardSectionsSnapshot *snapshot; /* Borrowed for this frame only. */
    int readonly, error;
    int pending;
    WenaId card_id;
    char key[WENA_SECTION_KEY_CAPACITY];
    unsigned long version;
    int collapsed;
} WenaCardSectionControl;
/* One widget in the caller's current row. No I/O. Uses exact section keys;
 * shared pending state keeps simultaneous views consistent within a frame.
 * Owner clears pending before drawing and applies a captured intent once after. */
int wena_card_section_toggle(struct nk_context *context,WenaCardSectionControl *control,
    const char *board,const char *card,const char *key);
#endif
