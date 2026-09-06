#ifndef WENA_SERVER_DOMAIN_OPERATION_H
#define WENA_SERVER_DOMAIN_OPERATION_H

#include "region_response.h"
#include "router.h"

#define WENA_DOMAIN_BODY_CAPACITY 4097u

typedef enum WenaDomainOperation {
    WENA_DOMAIN_CREATE_CARD = 1,
    WENA_DOMAIN_EDIT_CARD_TITLE = 2,
    WENA_DOMAIN_ARCHIVE_CARD = 3
} WenaDomainOperation;

typedef struct WenaDomainCommand {
    WenaDomainOperation operation;
    char user_id[65];
    char route[257];
    char form_body[WENA_DOMAIN_BODY_CAPACITY];
    size_t form_body_length;
} WenaDomainCommand;

typedef int (*WenaDomainApply)(void *context, const WenaDomainCommand *command,
                               WenaRegionResponse *response);

typedef struct WenaDomainAdapter {
    WenaDomainApply apply;
    void *context;
    unsigned long last_request_version;
} WenaDomainAdapter;

void wena_domain_adapter_init(WenaDomainAdapter *adapter, WenaDomainApply apply,
                              void *context);
int wena_domain_operation_dispatch(WenaDomainAdapter *adapter,
                                   const WenaRouteIntent *verified_intent,
                                   unsigned long request_version,
                                   WenaRegionResponse *response);

#endif
