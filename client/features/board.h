#ifndef WENA_BOARD_FEATURE_H
#define WENA_BOARD_FEATURE_H

#include "../components/boards/board_layout.h"
#include "card_details.h"

struct nk_context;

int wena_board_feature_render(struct nk_context *context,
                              const WenaBoardLayout *layout,
                              float width, float height);
int wena_board_feature_render_with_state(struct nk_context *context,
                                         const WenaBoardLayout *layout,
                                         float width, float height,
                                         WenaCardDetailsState *card_details);

#endif
