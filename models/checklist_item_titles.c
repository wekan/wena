#include "checklist_item_titles.h"
#include "text.h"
#include <string.h>

int wena_checklist_item_batch_parse(const char *text, size_t length,
    char titles[WENA_CHECKLIST_BATCH_MAX_ITEMS][WENA_CHECKLIST_TITLE_CAPACITY],
    size_t *count)
{
    char parsed[WENA_CHECKLIST_BATCH_MAX_ITEMS][WENA_CHECKLIST_TITLE_CAPACITY];
    size_t index, total;
    unsigned char byte;
    if (!count) return 0;
    *count = 0;
    if (!text || !titles || length > WENA_CHECKLIST_BATCH_MAX_BYTES) return 0;
    for (index = 0; index < length; ++index) {
        byte = (unsigned char)text[index];
        if ((byte < 32 && byte != 9 && byte != 10 && byte != 13) || byte == 127)
            return 0;
    }
    if (!wena_checklist_item_titles_parse(text, length, 1, 0, parsed,
        WENA_CHECKLIST_BATCH_MAX_ITEMS, &total) || !total) return 0;
    for (index = 0; index < total; ++index)
        if (!wena_model_title_string_valid(parsed[index],
            WENA_CHECKLIST_TITLE_CAPACITY)) return 0;
    for (index = 0; index < total; ++index) strcpy(titles[index], parsed[index]);
    *count = total;
    return 1;
}

int wena_checklist_item_titles_parse(const char *text, size_t length,
    int split_newlines, int reverse,
    char (*titles)[WENA_CHECKLIST_TITLE_CAPACITY], size_t capacity, size_t *count)
{
    size_t pass, offset, end, first, total, output, amount;
    if (!count) return 0;
    *count = 0;
    if ((!text && length) || length > WENA_CHECKLIST_ENTRY_MAX_BYTES ||
        capacity > WENA_CHECKLIST_MAX_ITEMS || (!titles && capacity) ||
        (split_newlines != 0 && split_newlines != 1) ||
        (reverse != 0 && reverse != 1)) return 0;
    total = 0;
    /* Validate all candidates before writing any output. LF is an ASCII byte,
     * so finding it cannot split a valid multi-byte UTF-8 scalar. Each whole
     * candidate is then validated by the shared trim helper. */
    for (pass = 0; pass < 2; ++pass) {
        offset = 0; output = 0;
        for (;;) {
            end = offset;
            while (end < length && (!split_newlines || text[end] != '\n')) ++end;
            if (!wena_model_text_trim_bounds(text ? text + offset : NULL,
                end - offset, &first, &amount)) return 0;
            if (amount) {
                if (pass == 0) {
                    if (amount >= WENA_CHECKLIST_TITLE_CAPACITY ||
                        total >= capacity) return 0;
                    ++total;
                } else {
                    size_t index;
                    index = split_newlines && reverse ? total - output - 1 : output;
                    memcpy(titles[index], text + offset + first, amount);
                    titles[index][amount] = '\0';
                    ++output;
                }
            }
            if (end == length) break;
            offset = end + 1;
        }
    }
    *count = total;
    return 1;
}
