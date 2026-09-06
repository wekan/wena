#ifndef WENA_I18N_LOCALE_H
#define WENA_I18N_LOCALE_H

#include <stddef.h>

int wena_locale_normalize(const char *input, char *output, size_t capacity);
int wena_locale_detect(char *output, size_t capacity);
int wena_locale_resolve(const char *requested, const char *const *available,
                        size_t available_count, char *output, size_t capacity);
int wena_locale_is_rtl(const char *language_tag);

#endif
