#include "region_response.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static int wena_safe_suffix(const char *value)
{
    if (*value == '\0') return 0;
    while (*value != '\0') {
        if (!isalnum((unsigned char)*value) && *value != '-' && *value != '_') return 0;
        ++value;
    }
    return 1;
}

int wena_region_name_valid(const char *name)
{
    static const char *const prefixes[] = {"card-", "list-", "swimlane-"};
    size_t index;
    size_t name_length;
    if (name == NULL) return 0;
    for (name_length = 0; name_length < WENA_REGION_NAME_CAPACITY &&
         name[name_length] != '\0'; ++name_length) {}
    if (name_length == WENA_REGION_NAME_CAPACITY) return 0;
    if (strcmp(name, "board") == 0 || strcmp(name, "sidebar") == 0 ||
        strcmp(name, "card-details") == 0) return 1;
    for (index = 0; index < sizeof(prefixes) / sizeof(prefixes[0]); ++index) {
        size_t length;
        length = strlen(prefixes[index]);
        if (strncmp(name, prefixes[index], length) == 0)
            return wena_safe_suffix(name + length);
    }
    return 0;
}

static int wena_utf8(const char *value, size_t length)
{
    size_t index;
    index = 0;
    while (index < length) {
        unsigned char first;
        unsigned long codepoint;
        size_t continuation;
        size_t offset;
        first = (unsigned char)value[index];
        if (first == 0 || first == 0x7f || (first < 0x20 && first != '\n' && first != '\t')) return 0;
        if (first < 0x80) { ++index; continue; }
        if (first >= 0xc2 && first <= 0xdf) { codepoint = first & 0x1f; continuation = 1; }
        else if (first >= 0xe0 && first <= 0xef) { codepoint = first & 0x0f; continuation = 2; }
        else if (first >= 0xf0 && first <= 0xf4) { codepoint = first & 0x07; continuation = 3; }
        else return 0;
        if (index + continuation >= length) return 0;
        for (offset = 1; offset <= continuation; ++offset) {
            unsigned char next;
            next = (unsigned char)value[index + offset];
            if ((next & 0xc0) != 0x80) return 0;
            codepoint = (codepoint << 6) | (next & 0x3f);
        }
        if ((continuation == 2 && codepoint < 0x800) ||
            (continuation == 3 && codepoint < 0x10000) ||
            (codepoint >= 0xd800 && codepoint <= 0xdfff) || codepoint > 0x10ffff) return 0;
        index += continuation + 1;
    }
    return 1;
}

static int wena_region_valid(const WenaRegion *region)
{
    return wena_region_name_valid(region->name) && region->version > 0ul &&
           region->content_length > 0 &&
           region->content_length < WENA_REGION_CONTENT_CAPACITY &&
           region->content[region->content_length] == '\0' &&
           wena_utf8(region->content, region->content_length);
}

int wena_region_response_encode(const WenaRegionResponse *response,
                                char *output, size_t capacity, size_t *length)
{
    char prefix[80];
    char header[160];
    size_t total;
    size_t used;
    size_t index;
    size_t header_length;
    size_t prefix_length;
    int written;

    if (length != NULL) *length = 0;
    if (response == NULL || output == NULL || length == NULL || capacity == 0 ||
        response->request_version == 0 || response->region_count > WENA_REGION_MAX_COUNT)
        return 0;
    written = sprintf(prefix, "WENA-REGIONS/1\nrequest-version %lu\n",
                      response->request_version);
    if (written < 0) return 0;
    prefix_length = (size_t)written;
    total = prefix_length + 4;
    /* Validate every region and the entire wire size before touching output.
     * In particular, even capacity=1 cannot receive an unchecked sprintf. */
    for (index = 0; index < response->region_count; ++index) {
        if (!wena_region_valid(&response->regions[index])) return 0;
        written = sprintf(header, "region %s %lu %lu\n", response->regions[index].name,
                          response->regions[index].version,
                          (unsigned long)response->regions[index].content_length);
        if (written < 0) return 0;
        total += (size_t)written + response->regions[index].content_length + 1;
    }
    if (total > capacity || total > WENA_REGION_RESPONSE_MAX_BYTES) return 0;
    memcpy(output, prefix, prefix_length);
    used = prefix_length;
    for (index = 0; index < response->region_count; ++index) {
        written = sprintf(header, "region %s %lu %lu\n", response->regions[index].name,
                          response->regions[index].version,
                          (unsigned long)response->regions[index].content_length);
        header_length = (size_t)written;
        memcpy(output + used, header, header_length); used += header_length;
        memcpy(output + used, response->regions[index].content,
               response->regions[index].content_length);
        used += response->regions[index].content_length;
        output[used++] = '\n';
    }
    memcpy(output + used, "end\n", 4); used += 4;
    *length = used;
    return 1;
}

static int wena_number(const char *start, const char *end, unsigned long *number)
{
    unsigned long result;
    if (start == end) return 0;
    result = 0ul;
    while (start < end) {
        unsigned long digit;
        if (!isdigit((unsigned char)*start)) return 0;
        digit = (unsigned long)(*start++ - '0');
        if (result > (~0ul - digit) / 10ul) return 0;
        result = result * 10ul + digit;
    }
    if (result == 0ul) return 0;
    *number = result;
    return 1;
}

static const char *wena_line(const char *cursor, const char *end)
{
    return (const char *)memchr(cursor, '\n', (size_t)(end - cursor));
}

static int wena_region_response_parse_into(const char *input, size_t length,
                               WenaRegionResponse *response)
{
    const char *cursor;
    const char *end;
    const char *line_end;
    if (input == NULL || response == NULL || length > WENA_REGION_RESPONSE_MAX_BYTES ||
        memchr(input, '\0', length) != NULL) return 0;
    if (length < 15 || memcmp(input, "WENA-REGIONS/1\n", 15) != 0) return 0;
    cursor = input + 15;
    end = input + length;
    line_end = wena_line(cursor, end);
    if (line_end == NULL || line_end - cursor < 17 ||
        memcmp(cursor, "request-version ", 16) != 0 ||
        !wena_number(cursor + 16, line_end, &response->request_version)) return 0;
    cursor = line_end + 1;
    while (cursor < end && !(end - cursor == 4 && memcmp(cursor, "end\n", 4) == 0)) {
        WenaRegion *region;
        const char *name_end;
        const char *version_end;
        unsigned long content_length;
        size_t name_length;
        if (response->region_count >= WENA_REGION_MAX_COUNT ||
            end - cursor < 8 || memcmp(cursor, "region ", 7) != 0) return 0;
        line_end = wena_line(cursor, end);
        if (line_end == NULL) return 0;
        name_end = memchr(cursor + 7, ' ', (size_t)(line_end - cursor - 7));
        if (name_end == NULL) return 0;
        version_end = memchr(name_end + 1, ' ', (size_t)(line_end - name_end - 1));
        if (version_end == NULL || memchr(version_end + 1, ' ',
                                          (size_t)(line_end - version_end - 1)) != NULL) return 0;
        region = &response->regions[response->region_count];
        name_length = (size_t)(name_end - (cursor + 7));
        if (name_length == 0 || name_length >= sizeof(region->name)) return 0;
        memcpy(region->name, cursor + 7, name_length); region->name[name_length] = '\0';
        if (!wena_number(name_end + 1, version_end, &region->version) ||
            !wena_number(version_end + 1, line_end, &content_length) ||
            content_length >= WENA_REGION_CONTENT_CAPACITY) return 0;
        cursor = line_end + 1;
        if ((unsigned long)(end - cursor) < content_length + 1ul ||
            cursor[content_length] != '\n') return 0;
        memcpy(region->content, cursor, (size_t)content_length);
        region->content[content_length] = '\0';
        region->content_length = (size_t)content_length;
        if (!wena_region_valid(region)) return 0;
        ++response->region_count;
        cursor += content_length + 1ul;
    }
    return cursor < end && end - cursor == 4 && memcmp(cursor, "end\n", 4) == 0;
}

int wena_region_response_parse(const char *input, size_t length,
                               WenaRegionResponse *response)
{
    int result;
    if (response == NULL) return 0;
    memset(response, 0, sizeof(*response));
    result = wena_region_response_parse_into(input, length, response);
    if (!result) memset(response, 0, sizeof(*response));
    return result;
}

int wena_region_response_accept(const WenaRegionResponse *response,
                                unsigned long expected_request_version,
                                WenaRegionState *state)
{
    unsigned long versions[WENA_REGION_MAX_COUNT];
    size_t indexes[WENA_REGION_MAX_COUNT];
    size_t index;
    size_t visible;
    if (response == NULL || state == NULL || response->request_version != expected_request_version ||
        response->request_version <= state->last_request_version ||
        response->region_count > WENA_REGION_MAX_COUNT ||
        state->visible_count > WENA_REGION_MAX_COUNT) return 0;
    for (visible = 0; visible < state->visible_count; ++visible) {
        size_t previous;
        if (!wena_region_name_valid(state->visible[visible].name)) return 0;
        for (previous = 0; previous < visible; ++previous)
            if (strcmp(state->visible[previous].name, state->visible[visible].name) == 0) return 0;
    }
    for (index = 0; index < response->region_count; ++index) {
        int found;
        size_t previous;
        if (!wena_region_valid(&response->regions[index])) return 0;
        for (previous = 0; previous < index; ++previous)
            if (strcmp(response->regions[previous].name, response->regions[index].name) == 0) return 0;
        found = 0;
        for (visible = 0; visible < state->visible_count; ++visible) {
            if (strcmp(state->visible[visible].name, response->regions[index].name) == 0) {
                if (response->regions[index].version <= state->visible[visible].version) return 0;
                indexes[index] = visible;
                versions[index] = response->regions[index].version;
                found = 1;
                break;
            }
        }
        if (!found) return 0;
    }
    for (index = 0; index < response->region_count; ++index)
        state->visible[indexes[index]].version = versions[index];
    state->last_request_version = response->request_version;
    return 1;
}
