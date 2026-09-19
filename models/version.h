#ifndef WENA_MODEL_VERSION_H
#define WENA_MODEL_VERSION_H

#include <limits.h>

/* Persisted entity versions start at one. The terminal version stays readable,
 * but cannot be supplied to another mutation, including a same-value request.
 * Leave room for every accepted mutation to produce a readable version on
 * platforms where long is narrower than SQLite's signed 64-bit integer.
 * Request identities, timestamps and ordering positions use separate bounds. */
#define WENA_VERSION_READ_MAX ((unsigned long)LONG_MAX - 1UL)
#define WENA_VERSION_MUTATE_MAX (WENA_VERSION_READ_MAX - 1UL)

#endif
