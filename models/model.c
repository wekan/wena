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

int wena_model_identifier_valid(const char *text)
{
    size_t index;
    unsigned char c;

    if (text == NULL || text[0] == '\0') return 0;
    for (index = 0; index < WENA_ID_CAPACITY; ++index) {
        c = (unsigned char)text[index];
        if (c == 0) return 1;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_')) return 0;
    }
    return 0;
}

static int wena_model_text_valid(const char *text, size_t length, size_t capacity,
                                  int multiline)
{
    size_t index;
    unsigned long c, code, need, minimum;

    if (text == NULL || (!multiline && length == 0) || length >= capacity) return 0;
    index = 0;
    while (index < length) {
        c = (unsigned char)text[index++];
        if (c < 32 || c == 127) {
            if (multiline && (c == 9 || c == 10 || c == 13)) continue;
            return 0;
        }
        if (c < 128) continue;
        if (c >= 194 && c <= 223) {
            code = c & 31; need = 1; minimum = 128;
        } else if (c >= 224 && c <= 239) {
            code = c & 15; need = 2; minimum = 2048;
        } else if (c >= 240 && c <= 244) {
            code = c & 7; need = 3; minimum = 65536;
        } else return 0;
        while (need != 0) {
            if (index >= length) return 0;
            c = (unsigned char)text[index++];
            if ((c & 192) != 128) return 0;
            code = (code << 6) | (c & 63);
            --need;
        }
        if (code < minimum || code > 1114111 ||
            (code >= 55296 && code <= 57343) ||
            (code >= 128 && code <= 159)) return 0;
    }
    return 1;
}

int wena_model_title_valid(const char *text, size_t length, size_t capacity)
{
    return wena_model_text_valid(text, length, capacity, 0);
}

int wena_model_description_valid(const char *text, size_t length)
{
    return wena_model_text_valid(text, length, WENA_DESCRIPTION_CAPACITY, 1);
}

int wena_model_title_string_valid(const char *text, size_t capacity)
{
    size_t length;

    if (text == NULL) return 0;
    for (length = 0; length < capacity && text[length] != '\0'; ++length) {}
    return wena_model_title_valid(text, length, capacity);
}
