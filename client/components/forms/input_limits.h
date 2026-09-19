#ifndef WENA_NATIVE_INPUT_LIMITS_H
#define WENA_NATIVE_INPUT_LIMITS_H

/* Keep one complete UTF-8 scalar beyond a feature's valid byte limit, so an
 * overlong edit remains visible and fails validation instead of silently
 * discarding a 2..4-byte scalar and leaving a valid prefix saveable.
 * The supplied model capacity already includes its terminating NUL byte. */
#define WENA_NATIVE_UTF8_SCALAR_BYTES 4
#define WENA_NATIVE_EDIT_CAPACITY(model_capacity) \
    ((model_capacity) + WENA_NATIVE_UTF8_SCALAR_BYTES)

#endif
