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
    WENA_DOMAIN_DELETE_CHECKLIST_ITEM = 21,
    WENA_DOMAIN_CREATE_LABEL = 22,
    WENA_DOMAIN_EDIT_LABEL = 23,
    WENA_DOMAIN_DELETE_LABEL = 24,
    WENA_DOMAIN_ASSIGN_LABEL = 25,
    WENA_DOMAIN_UNASSIGN_LABEL = 26,
    WENA_DOMAIN_ADD_CHECKLIST_ITEMS = 27,
    WENA_DOMAIN_REORDER_CHECKLIST = 28,
    WENA_DOMAIN_REORDER_CHECKLIST_ITEM = 29,
    WENA_DOMAIN_SET_BOARD_CHECKLIST_COUNT = 30,
    WENA_DOMAIN_MOVE_CHECKLIST = 31,
    WENA_DOMAIN_MOVE_CHECKLIST_ITEM = 32,
    WENA_DOMAIN_SET_BOARD_PRESENTATION = 33,
    WENA_DOMAIN_ARCHIVE_LIST = 34,
    WENA_DOMAIN_RESTORE_LIST = 35,
    WENA_DOMAIN_SET_LIST_COLOR = 36,
    WENA_DOMAIN_SET_SWIMLANE_COLOR = 37,
    WENA_DOMAIN_EDIT_LIST_WIP = 38,
    WENA_DOMAIN_ARCHIVE_SWIMLANE = 39,
    WENA_DOMAIN_RESTORE_SWIMLANE = 40,
    WENA_DOMAIN_ARCHIVE_LIST_CARDS = 41,
    WENA_DOMAIN_ARCHIVE_SELECTED_CARDS = 42
} WenaDomainOperation;

/* Native-only bounded batch payload. Storage remains caller-owned for apply.
 * HTTP dispatch never populates this span or exposes its operation. */
#define WENA_DOMAIN_CARD_BATCH_CAPACITY 2048u
typedef struct WenaDomainCardRevision {
    char id[65];
    unsigned long version;
} WenaDomainCardRevision;
typedef struct WenaDomainCommand {
    WenaDomainOperation operation;
    unsigned long request_version;
    char user_id[65];
    char route[257];
    char form_body[WENA_DOMAIN_BODY_CAPACITY];
    size_t form_body_length;
    const WenaDomainCardRevision *selected_cards;
    size_t selected_card_count;
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
