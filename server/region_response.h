#ifndef WENA_SERVER_REGION_RESPONSE_H
#define WENA_SERVER_REGION_RESPONSE_H

#include <stddef.h>

#define WENA_REGION_MAX_COUNT 8u
#define WENA_REGION_NAME_CAPACITY 65u
#define WENA_REGION_CONTENT_CAPACITY 4097u
#define WENA_REGION_RESPONSE_MAX_BYTES 32768u

typedef struct WenaRegion {
    char name[WENA_REGION_NAME_CAPACITY];
    unsigned long version;
    char content[WENA_REGION_CONTENT_CAPACITY];
    size_t content_length;
} WenaRegion;

typedef struct WenaRegionResponse {
    unsigned long request_version;
    WenaRegion regions[WENA_REGION_MAX_COUNT];
    size_t region_count;
} WenaRegionResponse;

typedef struct WenaVisibleRegion {
    char name[WENA_REGION_NAME_CAPACITY];
    unsigned long version;
} WenaVisibleRegion;

typedef struct WenaRegionState {
    unsigned long last_request_version;
    WenaVisibleRegion visible[WENA_REGION_MAX_COUNT];
    size_t visible_count;
} WenaRegionState;

int wena_region_name_valid(const char *name);
int wena_region_response_encode(const WenaRegionResponse *response,
                                char *output, size_t capacity, size_t *length);
int wena_region_response_parse(const char *input, size_t length,
                               WenaRegionResponse *response);
int wena_region_response_accept(const WenaRegionResponse *response,
                                unsigned long expected_request_version,
                                WenaRegionState *state);

#endif
