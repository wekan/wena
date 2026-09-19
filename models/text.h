#ifndef WENA_MODEL_TEXT_H
#define WENA_MODEL_TEXT_H

#include <stddef.h>

/* Locate the substring remaining after ECMAScript String.trim whitespace.
 * Validates the complete explicit-length input as strict scalar UTF-8 without
 * NUL. Empty input (including NULL with zero length) succeeds with zero bounds.
 * Other controls remain valid here: each feature applies its own text policy.
 * Output pointers must be distinct and must not overlap input. Invalid input
 * leaves both output values unchanged. No allocation or output text copying. */
int wena_model_text_trim_bounds(const char *input, size_t length,
    size_t *start, size_t *trimmed_length);

#endif
