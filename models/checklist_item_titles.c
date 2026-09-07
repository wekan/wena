#include "checklist_item_titles.h"
#include <string.h>

/* Decode strict scalar UTF-8: reject overlong encodings, surrogates, NUL and
 * out-of-range codepoints. Input length is explicit, never scanned unbounded. */
static int decode(const char *text, size_t length, size_t *offset,
                  unsigned long *codepoint)
{
    unsigned char lead, byte;
    unsigned long value, minimum;
    size_t remaining, i;
    lead = (unsigned char)text[*offset];
    if (lead > 0 && lead < 128) {
        *codepoint = lead; ++*offset; return 1;
    }
    if (lead >= 194 && lead <= 223) {
        remaining = 1; value = lead & 31u; minimum = 128;
    } else if (lead >= 224 && lead <= 239) {
        remaining = 2; value = lead & 15u; minimum = 2048;
    } else if (lead >= 240 && lead <= 244) {
        remaining = 3; value = lead & 7u; minimum = 65536;
    } else return 0;
    if (remaining >= length - *offset) return 0;
    for (i = 1; i <= remaining; ++i) {
        byte = (unsigned char)text[*offset + i];
        if (byte < 128 || byte > 191) return 0;
        value = (value << 6) | (byte & 63u);
    }
    if (value < minimum || value > 1114111UL ||
        (value >= 55296UL && value <= 57343UL)) return 0;
    *offset += remaining + 1; *codepoint = value; return 1;
}

/* ECMAScript WhiteSpace and LineTerminator, excluding historical U+180E. */
static int whitespace(unsigned long cp)
{
    return (cp >= 9 && cp <= 13) || cp == 32 || cp == 160 || cp == 5760 ||
        (cp >= 8192 && cp <= 8202) || cp == 8232 || cp == 8233 ||
        cp == 8239 || cp == 8287 || cp == 12288 || cp == 65279;
}

int wena_checklist_item_titles_parse(const char *text, size_t length,
    int split_newlines, int reverse,
    char (*titles)[WENA_CHECKLIST_TITLE_CAPACITY], size_t capacity, size_t *count)
{
    size_t pass, offset, before, first, last, total, output, amount;
    unsigned long cp;
    int end;
    if (!count) return 0;
    *count = 0;
    if ((!text && length) || length > WENA_CHECKLIST_ENTRY_MAX_BYTES ||
        capacity > WENA_CHECKLIST_MAX_ITEMS || (!titles && capacity) ||
        (split_newlines != 0 && split_newlines != 1) ||
        (reverse != 0 && reverse != 1)) return 0;
    total = 0;
    /* Validate all candidates before writing any output. */
    for (pass = 0; pass < 2; ++pass) {
        offset = 0; first = length; last = 0; output = 0;
        do {
            before = offset;
            end = offset == length;
            cp = 0;
            if (!end && !decode(text, length, &offset, &cp)) return 0;
            if (end || (split_newlines && cp == 10)) {
                if (first != length) {
                    amount = last - first;
                    if (pass == 0) {
                        if (amount >= WENA_CHECKLIST_TITLE_CAPACITY ||
                            total >= capacity) return 0;
                        ++total;
                    } else {
                        size_t index;
                        index = split_newlines && reverse ? total - output - 1 : output;
                        memcpy(titles[index], text + first, amount);
                        titles[index][amount] = '\0';
                        ++output;
                    }
                }
                first = length; last = 0;
            } else if (!whitespace(cp)) {
                if (first == length) first = before;
                last = offset;
            }
        } while (!end);
    }
    *count = total;
    return 1;
}
