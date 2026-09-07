#include "../server/http.h"
#include "../server/region_response.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MUTATIONS 6000u
#define FIXTURE_CAPACITY 1024u

static unsigned long random_state = 0x57454e41ul;
static unsigned long cases;
static unsigned long http_successes;
static unsigned long region_successes;
static WenaHttpRequest empty_request;
static WenaRegionResponse empty_response;

static unsigned long random_value(void)
{
    random_state = (random_state * 1664525ul + 1013904223ul) & 0xfffffffful;
    return random_state;
}

static char *exact_copy(const char *source, size_t length)
{
    char *copy;
    copy = (char *)malloc(length == 0 ? 1 : length);
    assert(copy != NULL);
    if (length) memcpy(copy, source, length);
    return copy;
}

static void check_http(const char *source, size_t length)
{
    WenaHttpRequest *request;
    WenaHttpParseResult result;
    size_t index;
    size_t byte;
    char *input;

    input = exact_copy(source, length);
    request = (WenaHttpRequest *)malloc(sizeof(*request));
    assert(request);
    memset(request, 0xa5, sizeof(*request));
    result = wena_http_parse(input, length, request);
    ++cases;
    if (result != WENA_HTTP_PARSE_OK) {
        assert(result == WENA_HTTP_PARSE_INVALID ||
            result == WENA_HTTP_PARSE_INCOMPLETE || result == WENA_HTTP_PARSE_TOO_LARGE);
        assert(memcmp(request, &empty_request, sizeof(*request)) == 0);
    } else {
        ++http_successes;
        assert(request->header_count <= WENA_HTTP_MAX_HEADERS);
        assert(request->consumed <= length && request->body_length <= WENA_HTTP_MAX_BODY_BYTES);
        assert(request->body >= input && request->body <= input + length);
        assert((size_t)(request->body - input) + request->body_length == request->consumed);
        assert(!strcmp(request->method, "GET") || !strcmp(request->method, "POST"));
        assert(request->target[0] == '/' && strlen(request->target) <= WENA_HTTP_MAX_TARGET_BYTES);
        for (index = 0; index < request->header_count; ++index) {
            assert(memchr(request->headers[index].name, '\0', sizeof(request->headers[index].name)));
            assert(memchr(request->headers[index].value, '\0', sizeof(request->headers[index].value)));
            for (byte = 0; request->headers[index].value[byte]; ++byte) {
                unsigned char c;
                c = (unsigned char)request->headers[index].value[byte];
                assert((c >= 32 || c == '\t') && c != 127);
            }
        }
    }
    free(request); free(input);
}

static void check_regions(const char *source, size_t length)
{
    WenaRegionResponse *response;
    WenaRegionResponse *roundtrip;
    WenaRegionState state;
    WenaRegionState before;
    char *input;
    char *wire;
    size_t wire_length;
    size_t index;
    int result;

    input = exact_copy(source, length);
    response = (WenaRegionResponse *)malloc(sizeof(*response));
    roundtrip = (WenaRegionResponse *)malloc(sizeof(*roundtrip));
    wire = (char *)malloc(WENA_REGION_RESPONSE_MAX_BYTES);
    assert(response && roundtrip && wire);
    memset(response, 0xa5, sizeof(*response));
    result = wena_region_response_parse(input, length, response);
    ++cases;
    if (!result) assert(memcmp(response, &empty_response, sizeof(*response)) == 0);
    else {
        ++region_successes;
        assert(response->region_count <= WENA_REGION_MAX_COUNT && response->request_version != 0);
        assert(wena_region_response_encode(response, wire, WENA_REGION_RESPONSE_MAX_BYTES, &wire_length));
        assert(wire_length <= WENA_REGION_RESPONSE_MAX_BYTES);
        assert(wena_region_response_parse(wire, wire_length, roundtrip));
        assert(roundtrip->request_version == response->request_version);
        assert(roundtrip->region_count == response->region_count);
        for (index = 0; index < response->region_count; ++index) {
            assert(!strcmp(roundtrip->regions[index].name, response->regions[index].name));
            assert(roundtrip->regions[index].version == response->regions[index].version);
            assert(roundtrip->regions[index].content_length == response->regions[index].content_length);
            assert(!memcmp(roundtrip->regions[index].content, response->regions[index].content,
                response->regions[index].content_length));
        }
        memset(&state, 0, sizeof(state));
        state.visible_count = 3;
        strcpy(state.visible[0].name, "board");
        strcpy(state.visible[1].name, "sidebar");
        strcpy(state.visible[2].name, "card-card_1");
        before = state;
        if (!wena_region_response_accept(response, response->request_version, &state))
            assert(!memcmp(&state, &before, sizeof(state)));
        else {
            before = state;
            assert(!wena_region_response_accept(response, response->request_version, &state));
            assert(!memcmp(&state, &before, sizeof(state)));
        }
        before = state;
        assert(!wena_region_response_accept(response,
            response->request_version == ULONG_MAX ? 0 : response->request_version + 1,
            &state));
        assert(!memcmp(&state, &before, sizeof(state)));
    }
    free(wire); free(roundtrip); free(response); free(input);
}

static size_t mutate(char *output, const char *seed, size_t length, unsigned int kind)
{
    size_t position;
    size_t count;
    unsigned int index;
    assert(length + 16 < FIXTURE_CAPACITY);
    memcpy(output, seed, length);
    position = (size_t)(random_value() % (unsigned long)(length + 1));
    if (kind == 0) {
        if (position < length) output[position] = (char)(random_value() & 255ul);
    } else if (kind == 1) {
        if (position < length) {
            memmove(output + position, output + position + 1, length - position - 1);
            --length;
        }
    } else if (kind == 2) {
        memmove(output + position + 1, output + position, length - position);
        output[position] = (char)(random_value() & 255ul); ++length;
    } else if (kind == 3) length = position;
    else if (kind == 4) {
        count = (size_t)(random_value() % 16ul);
        memmove(output + position + count, output + position, length - position);
        memset(output + position, '9', count); length += count;
    } else {
        for (index = 0; index < 8 && length; ++index)
            output[random_value() % (unsigned long)length] = (char)(random_value() & 255ul);
    }
    return length;
}

static void region_fixture(WenaRegionResponse *response)
{
    memset(response, 0, sizeof(*response));
    response->request_version = 10;
    response->region_count = 2;
    strcpy(response->regions[0].name, "board"); response->regions[0].version = 2;
    strcpy(response->regions[0].content, "Kortti \303\244\n\tUpdated");
    response->regions[0].content_length = strlen(response->regions[0].content);
    strcpy(response->regions[1].name, "sidebar"); response->regions[1].version = 4;
    strcpy(response->regions[1].content, "Activities");
    response->regions[1].content_length = strlen(response->regions[1].content);
}

static void targeted_regressions(void)
{
    WenaRegionResponse *response;
    WenaRegionResponse *parsed;
    WenaRegionState state;
    WenaRegionState before;
    WenaHttpRequest request;
    char wire[FIXTURE_CAPACITY];
    char header[128];
    char unterminated_name[WENA_REGION_NAME_CAPACITY];
    char *tiny;
    char *large;
    size_t length;
    size_t capacity;
    size_t result_length;
    size_t index;
    int c;

    response = (WenaRegionResponse *)malloc(sizeof(*response));
    parsed = (WenaRegionResponse *)malloc(sizeof(*parsed));
    assert(response && parsed);
    region_fixture(response);
    assert(wena_region_response_encode(response, wire, sizeof(wire), &length));
    for (capacity = 0; capacity < length; ++capacity) {
        tiny = (char *)malloc(capacity ? capacity : 1); assert(tiny);
        memset(tiny, 0x5a, capacity ? capacity : 1);
        result_length = 999;
        assert(!wena_region_response_encode(response, tiny, capacity, &result_length));
        assert(result_length == 0);
        for (index = 0; index < capacity; ++index) assert(tiny[index] == 0x5a);
        free(tiny);
    }
    tiny = (char *)malloc(length); assert(tiny);
    assert(wena_region_response_encode(response, tiny, length, &result_length));
    assert(length == result_length && !memcmp(tiny, wire, length)); free(tiny);
    memset(unterminated_name, 'a', sizeof(unterminated_name));
    assert(!wena_region_name_valid(unterminated_name));
    /* Failing after one valid region cannot expose the partial decoded object. */
    wire[length - 2] = '!';
    memset(parsed, 0xa5, sizeof(*parsed));
    assert(!wena_region_response_parse(wire, length, parsed));
    assert(!memcmp(parsed, &empty_response, sizeof(*parsed)));
    memset(parsed, 0xa5, sizeof(*parsed));
    assert(!wena_region_response_parse(NULL, 0, parsed));
    assert(!memcmp(parsed, &empty_response, sizeof(*parsed)));
    /* A stale later region must not apply an earlier eligible region. */
    memset(&state, 0, sizeof(state)); state.visible_count = 2;
    strcpy(state.visible[0].name, "board"); state.visible[0].version = 1;
    strcpy(state.visible[1].name, "sidebar"); state.visible[1].version = 4;
    before = state;
    assert(!wena_region_response_accept(response, 10, &state));
    assert(!memcmp(&before, &state, sizeof(state)));
    memset(&request, 0xa5, sizeof(request));
    assert(wena_http_parse(NULL, 0, &request) == WENA_HTTP_PARSE_INVALID);
    assert(!memcmp(&request, &empty_request, sizeof(request)));
    /* Bare line breaks and C0/DEL are rejected inside a field value; HTAB and
       HTTP obs-text remain accepted and are not confused with UTF-8 titles. */
    for (c = 1; c <= 255; ++c) {
        sprintf(header, "GET / HTTP/1.1\r\nHost: local\r\nX-Test: A%cB\r\n\r\n", c);
        if ((c < 32 && c != '\t') || c == 127)
            assert(wena_http_parse(header, strlen(header), &request) == WENA_HTTP_PARSE_INVALID);
        else assert(wena_http_parse(header, strlen(header), &request) == WENA_HTTP_PARSE_OK);
    }
    large = (char *)malloc(WENA_REGION_RESPONSE_MAX_BYTES + 1); assert(large);
    memset(large, 'x', WENA_REGION_RESPONSE_MAX_BYTES + 1);
    check_http(large, WENA_HTTP_MAX_REQUEST_BYTES + 1);
    check_regions(large, WENA_REGION_RESPONSE_MAX_BYTES + 1);
    sprintf(large, "POST / HTTP/1.1\r\nHost: x\r\nContent-Length: 8192\r\n\r\n");
    length = strlen(large); memset(large + length, 'x', 8192);
    assert(wena_http_parse(large, length + 8192, &request) == WENA_HTTP_PARSE_OK);
    assert(request.body_length == 8192 && request.consumed == length + 8192);
    check_http(large, length + 8191);
    free(large); free(parsed); free(response);
}

int main(void)
{
    const char *http_seeds[] = {
        "GET / HTTP/1.0\r\n\r\n",
        "GET /b/board/demo?x=%C3%A4 HTTP/1.1\r\nHost: localhost\r\nAccept: text/html\r\n\r\n",
        "POST /b/board/demo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 11\r\n\r\ntitle=hello",
        "GET / HTTP/1.1\r\nHost: localhost\r\n\r\nGET / HTTP/1.0\r\n\r\n"
    };
    const char *region_bad[] = {
        "WENA-REGIONS/1\nrequest-version 184467440737095516160\nend\n",
        "WENA-REGIONS/1\nrequest-version 1\nregion board 2 99999999999999999999999999\nx\nend\n",
        "WENA-REGIONS/1\nrequest-version 1\nregion board 2 2\n\300\257\nend\n",
        "WENA-REGIONS/1\nrequest-version 1\nregion board 2 3\n\355\240\200\nend\n",
        "WENA-REGIONS/1\nrequest-version 1\nregion board 2 4\n\364\220\200\200\nend\n"
    };
    WenaRegionResponse *response;
    char wire[FIXTURE_CAPACITY];
    char mutated[FIXTURE_CAPACITY];
    size_t length;
    size_t index;
    size_t seed;
    size_t changed;

    response = (WenaRegionResponse *)malloc(sizeof(*response)); assert(response);
    region_fixture(response);
    assert(wena_region_response_encode(response, wire, sizeof(wire), &length));
    targeted_regressions();
    for (seed = 0; seed < sizeof(http_seeds) / sizeof(http_seeds[0]); ++seed)
        for (index = 0; index <= strlen(http_seeds[seed]); ++index)
            check_http(http_seeds[seed], index);
    for (index = 0; index <= length; ++index) check_regions(wire, index);
    for (index = 0; index < sizeof(region_bad) / sizeof(region_bad[0]); ++index)
        check_regions(region_bad[index], strlen(region_bad[index]));
    for (index = 0; index < MUTATIONS; ++index) {
        seed = index % (sizeof(http_seeds) / sizeof(http_seeds[0]));
        changed = mutate(mutated, http_seeds[seed], strlen(http_seeds[seed]), (unsigned int)(index % 6));
        check_http(mutated, changed);
        changed = mutate(mutated, wire, length, (unsigned int)(index % 6));
        check_regions(mutated, changed);
    }
    assert(http_successes > 100 && region_successes > 100 && cases > 12000);
    printf("bounded parser mutation tests passed: %lu cases, %lu HTTP and %lu region accepts\n",
        cases, http_successes, region_successes);
    free(response);
    return 0;
}
