#include "../server/domain_operation.h"

#include <assert.h>
#include <string.h>

typedef struct FakeDomain { int calls; WenaDomainCommand last; int fail; } FakeDomain;

static int apply(void *context, const WenaDomainCommand *command,
                 WenaRegionResponse *response)
{
    FakeDomain *fake;
    fake = (FakeDomain *)context;
    ++fake->calls;
    fake->last = *command;
    if (fake->fail) return 0;
    response->region_count = 1u;
    strcpy(response->regions[0].name, "board");
    response->regions[0].version = 2ul;
    strcpy(response->regions[0].content, "updated");
    response->regions[0].content_length = strlen(response->regions[0].content);
    return 1;
}

int main(void)
{
    WenaDomainAdapter adapter;
    WenaRouteIntent intent;
    WenaRegionResponse response;
    FakeDomain fake;
    const char body[] = "legacyOperation=archive-card&title=Card";
    memset(&fake, 0, sizeof(fake));
    memset(&intent, 0, sizeof(intent));
    intent.result = WENA_ROUTE_MUTATION_INTENT;
    strcpy(intent.user_id, "user-1");
    strcpy(intent.route, "/b/board-1/demo");
    strcpy(intent.operation, "archive-card");
    intent.form_body = body;
    intent.form_body_length = strlen(body);
    wena_domain_adapter_init(&adapter, apply, &fake);
    assert(wena_domain_operation_dispatch(&adapter, &intent, 1ul, &response));
    assert(fake.calls == 1 && fake.last.operation == WENA_DOMAIN_ARCHIVE_CARD);
    assert(strcmp(fake.last.user_id, "user-1") == 0);
    assert(response.request_version == 1ul && response.region_count == 1u);
    assert(!wena_domain_operation_dispatch(&adapter, &intent, 1ul, &response));
    assert(fake.calls == 1);
    intent.result = WENA_ROUTE_REJECT;
    assert(!wena_domain_operation_dispatch(&adapter, &intent, 2ul, &response));
    intent.result = WENA_ROUTE_MUTATION_INTENT;
    intent.user_id[0] = '\0';
    assert(!wena_domain_operation_dispatch(&adapter, &intent, 2ul, &response));
    strcpy(intent.user_id, "user-1");
    strcpy(intent.route, "/admin");
    assert(!wena_domain_operation_dispatch(&adapter, &intent, 2ul, &response));
    strcpy(intent.route, "/b/board-1/demo");
    strcpy(intent.operation, "user-selected-function");
    assert(!wena_domain_operation_dispatch(&adapter, &intent, 2ul, &response));
    assert(fake.calls == 1);
    strcpy(intent.operation, "create-card");
    fake.fail = 1;
    assert(!wena_domain_operation_dispatch(&adapter, &intent, 2ul, &response));
    assert(fake.calls == 2 && adapter.last_request_version == 1ul);
    fake.fail = 0;
    strcpy(intent.operation, "edit-board-title");
    assert(wena_domain_operation_dispatch(&adapter, &intent, 2ul, &response));
    assert(fake.last.operation == WENA_DOMAIN_EDIT_BOARD_TITLE);
    strcpy(intent.operation, "edit-list-title");
    assert(wena_domain_operation_dispatch(&adapter, &intent, 3ul, &response));
    assert(fake.last.operation == WENA_DOMAIN_EDIT_LIST_TITLE);
    strcpy(intent.operation, "edit-swimlane-title");
    assert(wena_domain_operation_dispatch(&adapter, &intent, 4ul, &response));
    assert(fake.last.operation == WENA_DOMAIN_EDIT_SWIMLANE_TITLE);
    strcpy(intent.operation, "move-card");
    assert(wena_domain_operation_dispatch(&adapter, &intent, 5ul, &response));
    assert(fake.last.operation == WENA_DOMAIN_MOVE_CARD);
    strcpy(intent.operation, "move-list");
    assert(wena_domain_operation_dispatch(&adapter, &intent, 6ul, &response));
    assert(fake.last.operation == WENA_DOMAIN_MOVE_LIST);
    return 0;
}
