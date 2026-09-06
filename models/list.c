#include "list.h"

#include <string.h>

int wena_list_init(WenaList *list, const char *id, const char *board_id,
                   const char *swimlane_id, const char *title, double sort,
                   int archived)
{
    if (list == NULL) {
        return 0;
    }
    memset(list, 0, sizeof(*list));
    if (!wena_model_set_required(list->id, sizeof(list->id), id) ||
        !wena_model_set_required(list->board_id, sizeof(list->board_id),
                                 board_id) ||
        !wena_model_set_optional(list->swimlane_id,
                                 sizeof(list->swimlane_id), swimlane_id) ||
        !wena_model_set_required(list->title, sizeof(list->title), title)) {
        memset(list, 0, sizeof(*list));
        return 0;
    }
    list->sort = sort;
    list->archived = archived != 0;
    return 1;
}
