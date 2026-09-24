#ifndef WENA_CARD_SECTION_MODEL_H
#define WENA_CARD_SECTION_MODEL_H
#include "model.h"
#define WENA_SECTION_KEY_CAPACITY 129u
#define WENA_SECTIONS_PER_CARD 128u
typedef struct WenaCardSectionPreference {
    WenaId card_id;
    char key[WENA_SECTION_KEY_CAPACITY];
    int collapsed;
    unsigned long version;
} WenaCardSectionPreference;
typedef struct WenaCardSectionsSnapshot {
    WenaId actor_id, board_id;
    size_t count, capacity;
    WenaCardSectionPreference *entries;
} WenaCardSectionsSnapshot;
/* Keys match WeKan profile.collapsedCardSections, e.g. checklist-<id>.
 * Missing preference means expanded. All keys share the same guarded store. */
int wena_card_section_key_valid(const char *key);
int wena_card_section_checklist_key(const char *id, char *output, size_t capacity);
void wena_card_sections_free(WenaCardSectionsSnapshot *snapshot);
const WenaCardSectionPreference *wena_card_sections_find(
    const WenaCardSectionsSnapshot *snapshot,const char *card,const char *key);
int wena_card_section_compare(const char *card,const char *key,const WenaCardSectionPreference *entry);
#endif
