#ifndef WENA_SWIMLANE_RESIZE_H
#define WENA_SWIMLANE_RESIZE_H
#include "board_layout.h"
/* The resize bar drawn below each expanded swimlane (WenaBoardLayout's
 * swimlane_resize_bar): drag it to change the lane's height, release to store
 * it in layout->collapse, Escape to cancel. State is layout->swimlane_resize. */
void wena_swimlane_resize_bar(struct nk_context *context, const WenaBoardLayout *layout,
                              const WenaSwimlane *swimlane, unsigned int height);
#endif
