#ifndef WENA_SERVER_ROUTER_H
#define WENA_SERVER_ROUTER_H

#include "http.h"
#include "security.h"
#include "../imports/ui/page_contract.h"

typedef enum WenaRouteResult {
    WENA_ROUTE_REJECT = 0,
    WENA_ROUTE_READ_PAGE = 1,
    WENA_ROUTE_MUTATION_INTENT = 2
} WenaRouteResult;

typedef struct WenaRouteIntent {
    WenaRouteResult result;
    const WenaUiPageContract *page;
    char route[257];
    char operation[65];
    char user_id[65];
    const char *form_body;
    size_t form_body_length;
} WenaRouteIntent;

WenaRouteResult wena_route_dispatch(const WenaHttpRequest *request,
                                    WenaSecurityStore *security,
                                    unsigned long now,
                                    WenaRouteIntent *intent);

#endif
