#include "locale.h"

#include <ctype.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#endif

static int wena_copy(char *output, size_t capacity, const char *value)
{
    size_t length;
    if (output == NULL || capacity == 0 || value == NULL) {
        return 0;
    }
    length = strlen(value);
    if (length == 0 || length >= capacity) {
        output[0] = '\0';
        return 0;
    }
    memcpy(output, value, length + 1);
    return 1;
}

int wena_locale_normalize(const char *input, char *output, size_t capacity)
{
    size_t source;
    size_t destination;
    size_t segment;
    unsigned char character;

    if (output == NULL || capacity == 0) {
        return 0;
    }
    output[0] = '\0';
    if (input == NULL || input[0] == '\0' || strcmp(input, "C") == 0 ||
        strcmp(input, "POSIX") == 0) {
        return 0;
    }
    destination = 0;
    segment = 0;
    for (source = 0; input[source] != '\0' && input[source] != '.' &&
                     input[source] != '@'; ++source) {
        character = (unsigned char)input[source];
        if (character == '_' || character == '-') {
            if (segment == 0 || destination + 1 >= capacity) {
                output[0] = '\0';
                return 0;
            }
            output[destination++] = '-';
            segment = 0;
            continue;
        }
        if (!isalnum(character) || destination + 1 >= capacity) {
            output[0] = '\0';
            return 0;
        }
        output[destination++] = (char)tolower(character);
        ++segment;
    }
    if (segment == 0 || destination == 0) {
        output[0] = '\0';
        return 0;
    }
    output[destination] = '\0';
    {
        char *part;
        char *next;
        int part_index;
        part = output;
        part_index = 0;
        while (part != NULL) {
            size_t length;
            size_t index;
            next = strchr(part, '-');
            length = next == NULL ? strlen(part) : (size_t)(next - part);
            if (part_index > 0 && length == 2) {
                for (index = 0; index < length; ++index) {
                    part[index] = (char)toupper((unsigned char)part[index]);
                }
            } else if (part_index > 0 && length == 4) {
                part[0] = (char)toupper((unsigned char)part[0]);
            }
            part = next == NULL ? NULL : next + 1;
            ++part_index;
        }
    }
    return 1;
}

int wena_locale_detect(char *output, size_t capacity)
{
    const char *detected;
#if defined(_WIN32)
    wchar_t wide[LOCALE_NAME_MAX_LENGTH];
    char utf8[LOCALE_NAME_MAX_LENGTH * 4];
    int length;
    if (GetUserDefaultLocaleName(wide, LOCALE_NAME_MAX_LENGTH) > 0) {
        length = WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8,
                                     (int)sizeof(utf8), NULL, NULL);
        if (length > 0 && wena_locale_normalize(utf8, output, capacity)) {
            return 1;
        }
    }
#endif
    /* POSIX covers Linux/BSD/macOS/iOS/Android. AmigaOS and AROS builds use
       LANGUAGE/LANG when their optional locale.library integration is absent. */
    detected = setlocale(LC_ALL, "");
    if (detected != NULL && wena_locale_normalize(detected, output, capacity)) {
        return 1;
    }
    detected = getenv("LANGUAGE");
    if (detected == NULL || detected[0] == '\0') {
        detected = getenv("LC_ALL");
    }
    if (detected == NULL || detected[0] == '\0') {
        detected = getenv("LC_MESSAGES");
    }
    if (detected == NULL || detected[0] == '\0') {
        detected = getenv("LANG");
    }
    return wena_locale_normalize(detected, output, capacity);
}

static const char *wena_find(const char *candidate,
                             const char *const *available, size_t count)
{
    size_t index;
    for (index = 0; index < count; ++index) {
        if (strcmp(candidate, available[index]) == 0) {
            return available[index];
        }
    }
    return NULL;
}

int wena_locale_resolve(const char *requested, const char *const *available,
                        size_t available_count, char *output, size_t capacity)
{
    char normalized[64];
    char base[64];
    char *separator;
    const char *match;
    if (available == NULL ||
        !wena_locale_normalize(requested, normalized, sizeof(normalized))) {
        normalized[0] = '\0';
    }
    match = normalized[0] == '\0' ? NULL :
            wena_find(normalized, available, available_count);
    if (match == NULL && normalized[0] != '\0') {
        wena_copy(base, sizeof(base), normalized);
        separator = strchr(base, '-');
        if (separator != NULL) {
            *separator = '\0';
            match = wena_find(base, available, available_count);
        }
    }
    if (match == NULL) {
        match = wena_find("en", available, available_count);
    }
    return match != NULL && wena_copy(output, capacity, match);
}

int wena_locale_is_rtl(const char *language_tag)
{
    char normalized[64];
    char *separator;
    static const char *const rtl[] = {
        "ar", "arc", "ckb", "dv", "fa", "he", "ks", "ku", "ps", "sd", "ug", "ur", "yi"
    };
    size_t index;
    if (!wena_locale_normalize(language_tag, normalized, sizeof(normalized))) {
        return 0;
    }
    separator = strchr(normalized, '-');
    if (separator != NULL) {
        *separator = '\0';
    }
    for (index = 0; index < sizeof(rtl) / sizeof(rtl[0]); ++index) {
        if (strcmp(normalized, rtl[index]) == 0) {
            return 1;
        }
    }
    return 0;
}
