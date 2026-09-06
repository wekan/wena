#ifndef WENA_LIST_HEADER_H
#define WENA_LIST_HEADER_H

#include "../../../models/list.h"

struct nk_context;

#define WENA_LIST_HEADER_NO_ACTION 0u
#define WENA_LIST_HEADER_ADD_CARD 1u
#define WENA_LIST_HEADER_OPEN_MENU 2u

unsigned int wena_list_header_render(struct nk_context *context,
                                     const WenaList *list);

#endif
