#ifndef WENA_I18N_UI_CATALOG_H
#define WENA_I18N_UI_CATALOG_H

#include "language.h"

/* Exact canonical locale tags for use with the existing language state API.
 * Both this array and returned UTF-8 translations have static lifetime. */
const char *const *wena_ui_catalog_languages(size_t *count);

/* Canonical English fallback for unknown locale or empty translation.
 * Unknown keys return NULL so callers may retain their existing fallback.
 * State is read on each lookup: language changes need no cache invalidation. */
const char *wena_ui_catalog_lookup(const WenaLanguageState *state,
                                  const char *key);

/* Adapter for an optional (context, key) UI translation callback. */
const char *wena_ui_catalog_translate(void *context, const char *key);

#endif
