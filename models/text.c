#include "text.h"

/* Shared strict scalar decoder, originally in checklist_item_titles.c. */
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

int wena_model_text_trim_bounds(const char *input, size_t length,
    size_t *start, size_t *trimmed_length)
{
    size_t offset, before, first, last;
    unsigned long cp;
    if ((!input && length) || !start || !trimmed_length || start == trimmed_length)
        return 0;
    offset = 0; first = length; last = 0;
    while (offset < length) {
        before = offset;
        if (!decode(input, length, &offset, &cp)) return 0;
        if (!whitespace(cp)) {
            if (first == length) first = before;
            last = offset;
        }
    }
    *start = first == length ? 0 : first;
    *trimmed_length = first == length ? 0 : last - first;
    return 1;
}
