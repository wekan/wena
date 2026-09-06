#include "../server/region_response.h"

#include <assert.h>
#include <string.h>

static void region(WenaRegion *item, const char *name, unsigned long version,
                   const char *content)
{
    memset(item, 0, sizeof(*item));
    strcpy(item->name, name);
    item->version = version;
    strcpy(item->content, content);
    item->content_length = strlen(content);
}

int main(void)
{
    WenaRegionResponse source;
    WenaRegionResponse parsed;
    WenaRegionState state;
    char wire[WENA_REGION_RESPONSE_MAX_BYTES];
    size_t length;

    memset(&source, 0, sizeof(source));
    source.request_version = 10ul;
    source.region_count = 2;
    region(&source.regions[0], "card-card_1", 2ul, "Kortti ä");
    region(&source.regions[1], "sidebar", 4ul, "Activity refreshed");
    assert(wena_region_response_encode(&source, wire, sizeof(wire), &length));
    assert(wena_region_response_parse(wire, length, &parsed));
    assert(parsed.request_version == 10ul && parsed.region_count == 2);
    assert(strcmp(parsed.regions[0].content, "Kortti ä") == 0);

    memset(&state, 0, sizeof(state));
    state.last_request_version = 9ul;
    state.visible_count = 3;
    strcpy(state.visible[0].name, "card-card_1"); state.visible[0].version = 1ul;
    strcpy(state.visible[1].name, "sidebar"); state.visible[1].version = 3ul;
    strcpy(state.visible[2].name, "list-list_1"); state.visible[2].version = 7ul;
    assert(wena_region_response_accept(&parsed, 10ul, &state));
    assert(state.visible[0].version == 2ul && state.visible[1].version == 4ul);
    assert(state.visible[2].version == 7ul); /* partial response leaves it untouched */
    assert(!wena_region_response_accept(&parsed, 10ul, &state)); /* replay */

    state.last_request_version = 10ul;
    source.request_version = 12ul;
    source.region_count = 1;
    region(&source.regions[0], "card-card_1", 3ul, "newer");
    assert(!wena_region_response_accept(&source, 11ul, &state)); /* out of order */
    source.request_version = 11ul;
    source.regions[0].version = 2ul;
    assert(!wena_region_response_accept(&source, 11ul, &state)); /* stale region */
    source.regions[0].version = 3ul;
    strcpy(source.regions[0].name, "card-hidden");
    assert(!wena_region_response_accept(&source, 11ul, &state)); /* not visible */
    strcpy(source.regions[0].name, "card-card_1");
    strcpy(state.visible[1].name, "card-card_1");
    assert(!wena_region_response_accept(&source, 11ul, &state)); /* ambiguous visible state */
    strcpy(state.visible[1].name, "sidebar");
    strcpy(source.regions[0].name, "unknown-region");
    assert(!wena_region_response_encode(&source, wire, sizeof(wire), &length));

    assert(!wena_region_response_parse("broken", 6, &parsed));
    assert(!wena_region_response_parse("WENA-REGIONS/2\nrequest-version 1\nend\n",
        strlen("WENA-REGIONS/2\nrequest-version 1\nend\n"), &parsed));
    assert(!wena_region_name_valid("card-../../admin"));
    assert(!wena_region_name_valid("script"));
    source.request_version = 13ul;
    source.region_count = 1;
    region(&source.regions[0], "board", 1ul, "x");
    assert(!wena_region_response_parse(
        "WENA-REGIONS/1\nrequest-version 1\nregion board 1 4097\n",
        strlen("WENA-REGIONS/1\nrequest-version 1\nregion board 1 4097\n"), &parsed));
    memset(wire, 'x', sizeof(wire));
    assert(!wena_region_response_parse(wire, sizeof(wire), &parsed));

    source.regions[0].content[0] = (char)0xc0;
    source.regions[0].content[1] = (char)0x80;
    source.regions[0].content[2] = '\0';
    source.regions[0].content_length = 2;
    assert(!wena_region_response_encode(&source, wire, sizeof(wire), &length));
    return 0;
}
