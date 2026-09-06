#include "model.h"

#include <string.h>

static int wena_model_set(char *destination, size_t capacity,
                          const char *source, int required)
{
    size_t length;

    if (destination == NULL || capacity == 0 || source == NULL) {
        return 0;
    }
    length = strlen(source);
    if ((required && length == 0) || length >= capacity) {
        destination[0] = '\0';
        return 0;
    }
    memcpy(destination, source, length + 1);
    return 1;
}

int wena_model_set_required(char *destination, size_t capacity,
                            const char *source)
{
    return wena_model_set(destination, capacity, source, 1);
}

int wena_model_set_optional(char *destination, size_t capacity,
                            const char *source)
{
    return wena_model_set(destination, capacity, source, 0);
}
