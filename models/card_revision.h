#ifndef WENA_CARD_REVISION_H
#define WENA_CARD_REVISION_H
#include "model.h"
/* Immutable identity/revision captured before a confirmed bulk action. */
typedef struct WenaCardRevision { WenaId id; unsigned long version; } WenaCardRevision;
#endif
