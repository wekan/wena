#ifndef WENA_HIERARCHY_DESTINATION_H
#define WENA_HIERARCHY_DESTINATION_H
#include "../boards/board_layout.h"
/* Shared cached destination eligibility: archived/foreign parents are omitted;
 * lane-scoped lists appear only in that lane, board-wide lists in every lane. */
const WenaSwimlane *wena_hierarchy_destination_lane(const WenaBoardLayout*,const char*);
const WenaList *wena_hierarchy_destination_list(const WenaBoardLayout*,const char*,const char*);
/* Render exact-ID lane/list selectors. Caller owns fixed-size ID buffers and
 * confirmation state. Invalidated IDs require explicit repair; no automatic
 * destination changes or persistence writes. Returns current pair validity. */
int wena_hierarchy_destination_render(struct nk_context*,const WenaBoardLayout*,WenaId list,WenaId lane);
#endif
