#include "../models/text.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void trim(const char *input, size_t length, size_t expected_start,
                 size_t expected_length)
{
    size_t start, amount;
    start = 99; amount = 77;
    assert(wena_model_text_trim_bounds(input, length, &start, &amount));
    assert(start == expected_start && amount == expected_length);
}

static void reject(const char *input, size_t length)
{
    size_t start, amount;
    start = 99; amount = 77;
    assert(!wena_model_text_trim_bounds(input, length, &start, &amount));
    assert(start == 99 && amount == 77);
}

int main(void)
{
    static const char *const spaces[] = {
        "\t", "\n", "\v", "\f", "\r", " ", "\302\240", "\341\232\200",
        "\342\200\200", "\342\200\201", "\342\200\202", "\342\200\203",
        "\342\200\204", "\342\200\205", "\342\200\206", "\342\200\207",
        "\342\200\210", "\342\200\211", "\342\200\212", "\342\200\250",
        "\342\200\251", "\342\200\257", "\342\201\237", "\343\200\200",
        "\357\273\277"
    };
    static const char *const invalid[] = {
        "\200", "\277", "\300\257", "\301\277", "\302", "\302A",
        "\340\200\200", "\355\240\200", "\357\277", "\360\200\200\200",
        "\364\220\200\200", "\365\200\200\200", "\377"
    };
    char input[40], byte[1];
    size_t i, size, start, amount;
    trim(NULL, 0, 0, 0); trim("", 0, 0, 0);
    trim("  label  ", 9, 2, 5);
    trim("a\tb\nc", 5, 0, 5);
    trim("\341\240\216x\342\200\213", 7, 0, 7);
    trim("\302\205x\302\205", 5, 0, 5);
    trim("\360\237\230\200", 4, 0, 4);
    trim("\364\217\277\277", 4, 0, 4);
    for (i = 0; i < sizeof(spaces) / sizeof(spaces[0]); ++i) {
        size = strlen(spaces[i]);
        trim(spaces[i], size, 0, 0);
        memcpy(input, spaces[i], size);
        memcpy(input + size, "\303\204 label", 8);
        memcpy(input + size + 8, spaces[i], size);
        trim(input, size * 2 + 8, size, 8);
    }
    for (i = 1; i < 128; ++i) {
        byte[0] = (char)i;
        trim(byte, 1, 0, (i >= 9 && i <= 13) || i == 32 ? 0 : 1);
    }
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        reject(invalid[i], strlen(invalid[i]));
        strcpy(input, " x "); strcat(input, invalid[i]);
        reject(input, strlen(input));
    }
    reject("a\0b", 3); reject(NULL, 1);
    start = 99; amount = 77;
    assert(!wena_model_text_trim_bounds("a", 1, NULL, &amount));
    assert(!wena_model_text_trim_bounds("a", 1, &start, NULL));
    assert(!wena_model_text_trim_bounds("a", 1, &start, &start));
    assert(start == 99 && amount == 77);
    puts("shared text trim: exact ECMAScript whitespace and bounded UTF-8 passed");
    return 0;
}
