#ifndef WENA_SERVER_DOMAIN_OPERATION_H
#define WENA_SERVER_DOMAIN_OPERATION_H

#include "region_response.h"
#include "router.h"

#define WENA_DOMAIN_BODY_CAPACITY 4097u

typedef enum WenaDomainOperation {
    WENA_DOMAIN_CREATE_CARD = 1,
    WENA_DOMAIN_EDIT_CARD_TITLE = 2,
    WENA_DOMAIN_ARCHIVE_CARD = 3,
    WENA_DOMAIN_EDIT_BOARD_TITLE = 4,
    WENA_DOMAIN_EDIT_LIST_TITLE = 5,
    WENA_DOMAIN_EDIT_SWIMLANE_TITLE = 6,
    WENA_DOMAIN_MOVE_CARD = 7,
    WENA_DOMAIN_MOVE_LIST = 8,
    WENA_DOMAIN_MOVE_SWIMLANE = 9,
    WENA_DOMAIN_CREATE_LIST = 10,
    WENA_DOMAIN_CREATE_SWIMLANE = 11,
    WENA_DOMAIN_RESTORE_CARD = 12,
    WENA_DOMAIN_EDIT_CARD_DESCRIPTION = 13,
    WENA_DOMAIN_CREATE_CHECKLIST = 14,
    WENA_DOMAIN_RENAME_CHECKLIST = 15,
    WENA_DOMAIN_ADD_CHECKLIST_ITEM = 16,
    WENA_DOMAIN_RENAME_CHECKLIST_ITEM = 17,
    WENA_DOMAIN_SET_CHECKLIST_ITEM_FINISHED = 18,
    WENA_DOMAIN_SET_CHECKLIST_FLAGS = 19,
    WENA_DOMAIN_DELETE_CHECKLIST = 20,
    WENA_DOMAIN_DELETE_CHECKLIST_ITEM = 21
} WenaDomainOperation;

typedef struct WenaDomainCommand {
    WenaDomainOperation operation;
    unsigned long request_version;
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
