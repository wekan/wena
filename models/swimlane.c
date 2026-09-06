#include "swimlane.h"

#include <string.h>

int wena_swimlane_init(WenaSwimlane *swimlane, const char *id,
                       const char *board_id, const char *title, double sort,
                       int archived)
{
    if (swimlane == NULL) {
        return 0;
    }
    memset(swimlane, 0, sizeof(*swimlane));
    if (!wena_model_set_required(swimlane->id, sizeof(swimlane->id), id) ||
        !wena_model_set_required(swimlane->board_id,
                                 sizeof(swimlane->board_id), board_id) ||
        !wena_model_set_required(swimlane->title,
                                 sizeof(swimlane->title), title)) {
        memset(swimlane, 0, sizeof(*swimlane));
        return 0;
    }
    swimlane->sort = sort;
    swimlane->archived = archived != 0;
    return 1;
}
