#include "test_files.h"
#include "wena_test_files.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *base_name(const char *path)
{
    const char *name = path, *cursor;
    for (cursor = path; *cursor; ++cursor)
        if (*cursor == '/' || *cursor == '\\') name = cursor + 1;
    return name;
}

const char *wena_test_file(const char *path, size_t *size)
{
    size_t index;
    const char *name;
    if (path == NULL) abort();
    name = base_name(path);
    for (index = 0; index < WENA_TEST_FILE_COUNT; ++index) {
        if (strcmp(wena_test_files[index].name, name) == 0) {
            if (size != NULL) *size = wena_test_files[index].size;
            return (const char *)wena_test_files[index].data;
        }
    }
    fprintf(stderr, "test file %s was not embedded by its test script\n", name);
    abort();
    return NULL;
}

char *wena_test_file_copy(const char *path, long *size)
{
    size_t length;
    const char *data = wena_test_file(path, &length);
    char *copy = (char *)malloc(length + 1);
    if (copy == NULL) abort();
    memcpy(copy, data, length + 1);
    if (size != NULL) *size = (long)length;
    return copy;
}
