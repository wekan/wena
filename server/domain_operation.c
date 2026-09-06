#include "domain_operation.h"

#include <string.h>

static WenaDomainOperation wena_domain_operation(const char *value)
{
    if (value != NULL && strcmp(value, "create-card") == 0) return WENA_DOMAIN_CREATE_CARD;
    if (value != NULL && strcmp(value, "edit-card-title") == 0) return WENA_DOMAIN_EDIT_CARD_TITLE;
    if (value != NULL && strcmp(value, "archive-card") == 0) return WENA_DOMAIN_ARCHIVE_CARD;
    if (value != NULL && strcmp(value, "edit-board-title") == 0) return WENA_DOMAIN_EDIT_BOARD_TITLE;
    return (WenaDomainOperation)0;
}

void wena_domain_adapter_init(WenaDomainAdapter *adapter, WenaDomainApply apply,
                              void *context)
{
    if (adapter == NULL) return;
    memset(adapter, 0, sizeof(*adapter));
    adapter->apply = apply;
    adapter->context = context;
}

int wena_domain_operation_dispatch(WenaDomainAdapter *adapter,
                                   const WenaRouteIntent *verified_intent,
                                   unsigned long request_version,
                                   WenaRegionResponse *response)
{
    WenaDomainCommand command;
    WenaRegionResponse candidate;
    char encoded[WENA_REGION_RESPONSE_MAX_BYTES];
    size_t encoded_length;
    if (adapter == NULL || adapter->apply == NULL || verified_intent == NULL ||
        response == NULL || verified_intent->result != WENA_ROUTE_MUTATION_INTENT ||
        verified_intent->user_id[0] == '\0' || strncmp(verified_intent->route, "/b/", 3) != 0 ||
        request_version == 0ul || request_version <= adapter->last_request_version ||
        verified_intent->form_body == NULL || verified_intent->form_body_length == 0u ||
        verified_intent->form_body_length >= WENA_DOMAIN_BODY_CAPACITY) return 0;
    memset(&command, 0, sizeof(command));
    command.operation = wena_domain_operation(verified_intent->operation);
    if ((int)command.operation == 0) return 0;
    strcpy(command.user_id, verified_intent->user_id);
    command.request_version = request_version;
    strcpy(command.route, verified_intent->route);
    memcpy(command.form_body, verified_intent->form_body, verified_intent->form_body_length);
    command.form_body[verified_intent->form_body_length] = '\0';
    command.form_body_length = verified_intent->form_body_length;
    memset(&candidate, 0, sizeof(candidate));
    if (!adapter->apply(adapter->context, &command, &candidate)) return 0;
    candidate.request_version = request_version;
    if (!wena_region_response_encode(&candidate, encoded, sizeof(encoded), &encoded_length))
        return 0;
    adapter->last_request_version = request_version;
    *response = candidate;
    return 1;
}
