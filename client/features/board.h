#ifndef WENA_BOARD_FEATURE_H
#define WENA_BOARD_FEATURE_H

#include "../components/boards/board_layout.h"

struct nk_context;

int wena_board_feature_render(struct nk_context *context,
                              const WenaBoardLayout *layout,
                              float width, float height);

#endif
