#ifndef WENA_SERVER_RESPONSE_POLICY_H
#define WENA_SERVER_RESPONSE_POLICY_H

#include "settings.h"

#include <stddef.h>

#define WENA_RESPONSE_MAX_HEADERS 12u

typedef struct WenaResponseHeader {
    const char *name;
    char value[384];
} WenaResponseHeader;

typedef struct WenaResponsePolicy {
    WenaResponseHeader headers[WENA_RESPONSE_MAX_HEADERS];
    size_t header_count;
    int origin_allowed;
} WenaResponsePolicy;

int wena_response_policy(const WenaRootUrl *root, const char *origin,
                         WenaResponsePolicy *policy);
const char *wena_response_header(const WenaResponsePolicy *policy,
                                 const char *name);

#endif
