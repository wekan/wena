#ifndef WENA_MODEL_H
#define WENA_MODEL_H

#include <stddef.h>

#define WENA_ID_CAPACITY 65
#define WENA_TITLE_CAPACITY 257

typedef char WenaId[WENA_ID_CAPACITY];
typedef char WenaTitle[WENA_TITLE_CAPACITY];

int wena_model_set_required(char *destination, size_t capacity,
                            const char *source);
int wena_model_set_optional(char *destination, size_t capacity,
                            const char *source);

#endif
