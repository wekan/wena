#include "checklist_store.h"
#include <limits.h>
#include <string.h>

int wena_checklist_snapshot_valid(const WenaChecklistSnapshot *snapshot,
    const char *board_id, const char *card_id)
{
    size_t index, previous_index;
    const WenaChecklist *checklist, *parent;
    const WenaChecklistItem *item, *previous;
    if (snapshot == NULL ||
        !wena_model_identifier_valid(board_id) ||
        !wena_model_identifier_valid(card_id) ||
        !wena_model_identifier_valid(snapshot->board_id) ||
        !wena_model_identifier_valid(snapshot->card_id) ||
        strcmp(snapshot->board_id, board_id) != 0 ||
        strcmp(snapshot->card_id, card_id) != 0 ||
        snapshot->card_version == 0 ||
        snapshot->card_version > WENA_VERSION_READ_MAX ||
        snapshot->checklist_count > WENA_CARD_CHECKLIST_CAPACITY ||
        snapshot->item_count > WENA_CARD_CHECKLIST_ITEM_CAPACITY) return 0;
    for (index = 0; index < snapshot->checklist_count; ++index) {
        checklist = &snapshot->checklists[index];
        if (!wena_checklist_valid(checklist) ||
            snapshot->checklist_versions[index] == 0 ||
            snapshot->checklist_versions[index] > WENA_VERSION_READ_MAX ||
            strcmp(checklist->board_id, board_id) != 0 ||
            strcmp(checklist->card_id, card_id) != 0) return 0;
        if (index > 0 &&
            wena_checklist_compare(&snapshot->checklists[index - 1], checklist) >= 0) return 0;
        for (previous_index = 0; previous_index < index; ++previous_index) {
            if (strcmp(checklist->id,
                       snapshot->checklists[previous_index].id) == 0 ||
                checklist->position ==
                    snapshot->checklists[previous_index].position) return 0;
        }
    }
    for (index = 0; index < snapshot->item_count; ++index) {
        item = &snapshot->items[index];
        if (!wena_checklist_item_valid(item) ||
            snapshot->item_versions[index] == 0 ||
            snapshot->item_versions[index] > WENA_VERSION_READ_MAX) return 0;
        parent = NULL;
        for (previous_index = 0; previous_index < snapshot->checklist_count;
             ++previous_index) {
            if (strcmp(item->checklist_id,
                       snapshot->checklists[previous_index].id) == 0) {
                parent = &snapshot->checklists[previous_index];
            }
        }
        if (parent == NULL || !wena_checklist_item_validate_parent(item, parent)) return 0;
        if (index > 0) {
            previous = &snapshot->items[index - 1];
            if (strcmp(previous->checklist_id, item->checklist_id) > 0 ||
                (strcmp(previous->checklist_id, item->checklist_id) == 0 &&
                wena_checklist_item_compare(previous, item) >= 0)) return 0;
        }
        for (previous_index = 0; previous_index < index; ++previous_index) {
            previous = &snapshot->items[previous_index];
            if (strcmp(item->id, previous->id) == 0 ||
                (strcmp(item->checklist_id, previous->checklist_id) == 0 &&
                item->position == previous->position)) return 0;
        }
    }
    return 1;
}
