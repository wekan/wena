#include "../models/checklist_item_titles.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static char titles[WENA_CHECKLIST_MAX_ITEMS][WENA_CHECKLIST_TITLE_CAPACITY];
static void parse(const char *input, int split, int reverse, size_t expected,
                  const char *first, const char *second)
{
    size_t count;
    assert(wena_checklist_item_titles_parse(input, strlen(input), split,
        reverse, titles, WENA_CHECKLIST_MAX_ITEMS, &count));
    assert(count == expected);
    if (expected) assert(strcmp(titles[0], first) == 0);
    if (expected > 1) assert(strcmp(titles[1], second) == 0);
}
static void reject(const char *input, size_t length, int split, int reverse,
                   size_t capacity)
{
    size_t count;
    memset(titles, 85, sizeof(titles)); count = 42;
    assert(!wena_checklist_item_titles_parse(input, length, split, reverse,
        titles, capacity, &count));
    assert(count == 0);
    {
        size_t i;
        const unsigned char *bytes;
        bytes = (const unsigned char *)titles;
        for (i = 0; i < sizeof(titles); ++i) assert(bytes[i] == 85);
    }
}
int main(void)
{
    char maximum[WENA_CHECKLIST_TITLE_CAPACITY + 1];
    char many[WENA_CHECKLIST_MAX_ITEMS * 2 + 3];
    char utf8[WENA_CHECKLIST_TITLE_CAPACITY + 1];
    size_t count, i;
    parse("", 0, 0, 0, 0, 0);
    parse(" \t\r\n\v\f", 1, 1, 0, 0, 0);
    parse("  First  \r\n\n Second \n", 1, 0, 2, "First", "Second");
    parse("  First  \r\n\n Second \n", 1, 1, 2, "Second", "First");
    parse("  First\n Second \n", 0, 1, 1, "First\n Second", 0);
    parse("a\rb\tc", 1, 0, 1, "a\rb\tc", 0);
    parse("\357\273\277\302\240\343\200\200Finnish \303\204\342\200\257", 0, 0, 1, "Finnish \303\204", 0);
    parse("\342\200\250a\342\200\251b\342\200\250", 1, 0, 1, "a\342\200\251b", 0);
    parse("\341\240\216a\342\200\213", 0, 0, 1, "\341\240\216a\342\200\213", 0);
    parse("\302\205a\302\205", 0, 0, 1, "\302\205a\302\205", 0);
    parse("\341\232\200\342\200\200\342\200\201\342\200\202\342\200\203\342\200\204\342\200\205\342\200\206\342\200\207\342\200\210\342\200\211\342\200\212\342\201\237", 0, 0, 0, 0, 0);
    parse("a\n a\n", 1, 0, 2, "a", "a");
    parse("\360\237\230\200", 0, 0, 1, "\360\237\230\200", 0);
    memset(maximum, 'a', sizeof(maximum)); maximum[128] = 0;
    parse(maximum, 0, 0, 1, maximum, 0);
    maximum[128] = 'a'; maximum[129] = 0;
    reject(maximum, 129, 0, 0, 10);
    for (i = 0; i < 128; i += 2) { utf8[i] = (char)195; utf8[i+1] = (char)132; }
    utf8[128] = 0; parse(utf8, 0, 0, 1, utf8, 0);
    for (i = 0; i < WENA_CHECKLIST_MAX_ITEMS; ++i) { many[i*2] = 'a'; many[i*2+1] = '\n'; }
    many[i*2] = 0;
    assert(wena_checklist_item_titles_parse(many, i*2, 1, 1, titles, i, &count));
    assert(count == WENA_CHECKLIST_MAX_ITEMS);
    many[i*2] = 'b'; many[i*2+1] = 0;
    reject(many, i*2+1, 1, 0, WENA_CHECKLIST_MAX_ITEMS);
    reject("a\nb", 3, 1, 0, 1);
    reject("a\n\377", 3, 1, 0, 10);
    reject("a\0b", 3, 0, 0, 10);
    reject("\300\257", 2, 0, 0, 10);
    reject("\355\240\200", 3, 0, 0, 10);
    reject("\364\220\200\200", 4, 0, 0, 10);
    reject("\342\200", 2, 0, 0, 10);
    reject("a", 1, 2, 0, 10);
    reject("a", 1, 0, -1, 10);
    reject("a", 1, 0, 0, WENA_CHECKLIST_MAX_ITEMS+1);
    reject("a", WENA_CHECKLIST_ENTRY_MAX_BYTES+1, 0, 0, 10);
    reject(0, 1, 0, 0, 10);
    assert(wena_checklist_item_titles_parse(0, 0, 0, 0, 0, 0, &count) && count == 0);
    assert(!wena_checklist_item_titles_parse("a", 1, 0, 0, 0, 1, &count));
    assert(!wena_checklist_item_titles_parse("a", 1, 0, 0, titles, 1, 0));
    puts("checklist item titles: canonical trim/split/reverse and atomic bounded UTF-8 validation passed");
    return 0;
}
