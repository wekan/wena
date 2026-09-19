#include "../models/color.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static double channel(unsigned int value)
{
    double x;
    x = value / 255.0;
    return x <= 0.04045 ? x / 12.92 : pow((x + 0.055) / 1.055, 2.4);
}

static void contrast(unsigned int r, unsigned int g, unsigned int b)
{
    char hex[8];
    unsigned char rgb[3];
    unsigned char fg[3];
    double luminance;
    double black;
    double white;
    double actual;
    double best;
    sprintf(hex, "#%02x%02x%02x", r, g, b);
    assert(wena_color_rgb(hex, rgb));
    assert(rgb[0] == r && rgb[1] == g && rgb[2] == b);
    assert(wena_color_foreground(hex, fg));
    assert(fg[0] == fg[1] && fg[1] == fg[2]);
    assert(fg[0] == 0 || fg[0] == 255);
    luminance = .2126 * channel(r) + .7152 * channel(g) + .0722 * channel(b);
    black = (luminance + .05) / .05;
    white = 1.05 / (luminance + .05);
    actual = fg[0] == 0 ? black : white;
    best = black > white ? black : white;
    assert(actual >= 4.5);
    /* Fixed point rounding is bounded; compare to independent floating math. */
    assert(best - actual < .006);
}

int main(void)
{
    static const char *invalid[] = {
        "#fff", "#12345", "#1234567", "#12xz56", "#-12345", "#12345\n",
        "White", "WHITE", " white", "white ", "belize", "natural", "", "red\n",
        "#ff00ff\t", "#12345678901234567890", "transparent"
    };
    const WenaColorContract *palette;
    const WenaColorContract *all;
    unsigned char rgb[3];
    unsigned char expected[3];
    unsigned char fg[3];
    char unterminated[WENA_COLOR_CAPACITY];
    size_t count;
    size_t index;
    unsigned int r;
    unsigned int g;
    unsigned int b;
    palette = wena_colors(&count);
    assert(count == 25 && strcmp(palette[0].name, "white") == 0);
    all = wena_color_contracts(&count);
    assert(count == 50 && palette == all + 25);
    assert(wena_colors(NULL) == palette && wena_color_contracts(NULL) == all);
    for (index = 0; index < 25; ++index) {
        assert(wena_color_valid(palette[index].name));
        assert(wena_color_rgb(palette[index].name, rgb));
        assert(wena_color_rgb(palette[index].rgb, expected));
        assert(memcmp(rgb, expected, 3) == 0);
        contrast(rgb[0], rgb[1], rgb[2]);
    }
    assert(wena_color_rgb("", rgb));
    assert(rgb[0] == 255 && rgb[1] == 255 && rgb[2] == 255);
    assert(wena_color_rgb("#Aa09Ff", rgb));
    assert(rgb[0] == 170 && rgb[1] == 9 && rgb[2] == 255);
    assert(wena_color_foreground("#777777", fg) && fg[0] == 0);
    assert(wena_color_foreground("#000000", fg) && fg[0] == 255);
    assert(wena_color_foreground("#ffffff", fg) && fg[0] == 0);
    for (index = 0; index < sizeof(invalid) / sizeof(invalid[0]); ++index) {
        if (invalid[index][0] == '\0') continue;
        memset(rgb, 42, sizeof(rgb));
        assert(!wena_color_valid(invalid[index]));
        assert(!wena_color_rgb(invalid[index], rgb));
        assert(!wena_color_foreground(invalid[index], rgb));
        assert(rgb[0] == 42 && rgb[1] == 42 && rgb[2] == 42);
    }
    memset(unterminated, 'x', sizeof(unterminated));
    assert(!wena_color_valid(unterminated));
    assert(!wena_color_valid(NULL));
    assert(!wena_color_rgb("red", NULL));
    assert(!wena_color_foreground("red", NULL));
    for (r = 0; r < 256; r += 17) {
        for (g = 0; g < 256; g += 17) {
            for (b = 0; b < 256; b += 17) contrast(r, g, b);
        }
    }
    for (r = 0; r < 256; ++r) contrast(r, r, r);
    puts("colors: canonical palette, strict validation, 4377 independent contrast checks passed");
    return 0;
}
