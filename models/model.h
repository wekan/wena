#ifndef WENA_MODEL_H
#define WENA_MODEL_H

#include <stddef.h>
#include "version.h"

#define WENA_ID_CAPACITY 65
#define WENA_TITLE_CAPACITY 257
#define WENA_DESCRIPTION_CAPACITY 1025

typedef char WenaId[WENA_ID_CAPACITY];
typedef char WenaTitle[WENA_TITLE_CAPACITY];

/* Strict native/storage identifiers: 1..64 ASCII letters, digits, '_' or '-'.
 * These validators do not change the permissive bounded model-copy helpers. */
int wena_model_identifier_valid(const char *text);
/* Nonempty UTF-8 without C0, DEL or C1 controls; capacity is an exclusive
 * byte bound supplied by the caller (129 for edited titles, 257 for models).
 * The length-delimited form does not require a terminating NUL and rejects
 * embedded NUL. The string form scans at most capacity bytes for its NUL. */
int wena_model_title_valid(const char *text, size_t length, size_t capacity);
int wena_model_title_string_valid(const char *text, size_t capacity);
/* Empty or up to 1024 UTF-8 bytes; preserves spaces and LF/CR/TAB, rejects
 * embedded NUL and every other C0/DEL/C1 control. */
int wena_model_description_valid(const char *text, size_t length);

int wena_model_set_required(char *destination, size_t capacity,
                            const char *source);
int wena_model_set_optional(char *destination, size_t capacity,
                            const char *source);

#endif
