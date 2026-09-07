#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_IMPLEMENTATION
#include <nuklear.h>
#include "../client/platform/theme.h"
#include "../imports/ui/page_contract.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static float text_width(nk_handle handle, float height,
                        const char *text, int length)
{
    (void)handle; (void)text;
    return height * (float)length * 0.5f;
}
static int equal(struct nk_color a, struct nk_color b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}
static double channel(unsigned char c)
{
    double value;
    value = (double)c / 255.0;
    return value <= 0.04045 ? value / 12.92 : pow((value + 0.055) / 1.055, 2.4);
}
static double luminance(struct nk_color color)
{
    return 0.2126 * channel(color.r) + 0.7152 * channel(color.g) +
           0.0722 * channel(color.b);
}
static double contrast(struct nk_color a, struct nk_color b)
{
    double first, second;
    first = luminance(a); second = luminance(b);
    return first > second ? (first + 0.05) / (second + 0.05) :
                            (second + 0.05) / (first + 0.05);
}
int main(void)
{
    struct nk_context ctx;
    struct nk_user_font font;
    struct nk_color color, before, panel, ink, paper, accent, navy;
    const WenaUiColorContract *colors;
    const struct nk_command *command;
    const struct nk_command_text *text;
    const struct nk_command_rect_filled *rectangle;
    size_t count, i;
    unsigned int r, g, b;
    int saw_panel, saw_button, saw_text;
    colors = wena_ui_colors(&count);
    for (i = 0; i < count; ++i) {
        assert(wena_native_theme_color(colors[i].name, &color));
        assert(sscanf(colors[i].rgb, "#%2x%2x%2x", &r, &g, &b) == 3);
        assert(color.r == r && color.g == g && color.b == b && color.a == 255);
    }
    before = color;
    assert(!wena_native_theme_color("not-a-theme", &color));
    assert(equal(before, color));
    assert(!wena_native_theme_color(NULL, &color));
    assert(!wena_native_theme_color("white", NULL));
    assert(!wena_native_theme_apply(NULL));
    assert(wena_native_theme_color("cleanlight", &panel));
    assert(wena_native_theme_color("dark", &ink));
    assert(wena_native_theme_color("white", &paper));
    assert(wena_native_theme_color("belize", &accent));
    assert(wena_native_theme_color("midnight", &navy));
    memset(&font, 0, sizeof(font)); font.height = 14.0f; font.width = text_width;
    assert(nk_init_default(&ctx, &font));
    assert(wena_native_theme_apply(&ctx));
    assert(ctx.style.font == &font);
    assert(equal(ctx.style.window.background, panel));
    assert(equal(ctx.style.button.normal.data.color, paper));
    assert(equal(ctx.style.scrollv.cursor_normal.data.color, accent));
    assert(contrast(ctx.style.text.color, ctx.style.window.background) >= 4.5);
    assert(contrast(ctx.style.button.text_normal, ctx.style.button.normal.data.color) >= 4.5);
    assert(contrast(ctx.style.button.text_hover, ctx.style.button.hover.data.color) >= 4.5);
    assert(contrast(ctx.style.button.text_active, ctx.style.button.active.data.color) >= 4.5);
    assert(contrast(ctx.style.window.header.label_normal, ctx.style.window.header.normal.data.color) >= 4.5);
    assert(contrast(ctx.style.selectable.text_normal_active, ctx.style.selectable.normal_active.data.color) >= 4.5);
    if (nk_begin(&ctx, "Theme", nk_rect(0, 0, 400, 300), NK_WINDOW_TITLE)) {
        nk_layout_row_dynamic(&ctx, 24, 1);
        nk_label(&ctx, "Readable title", NK_TEXT_LEFT);
        nk_button_label(&ctx, "Card action");
    }
    nk_end(&ctx);
    saw_panel = saw_button = saw_text = 0;
    nk_foreach(command, &ctx) {
        if (command->type == NK_COMMAND_RECT_FILLED) {
            rectangle = (const struct nk_command_rect_filled *)command;
            if (equal(rectangle->color, panel)) saw_panel = 1;
            if (equal(rectangle->color, paper)) saw_button = 1;
        }
        if (command->type == NK_COMMAND_TEXT) {
            text = (const struct nk_command_text *)command;
            if (text->length == 14 && memcmp(text->string, "Readable title", 14) == 0) {
                assert(equal(text->foreground, ink));
                saw_text = 1;
            }
        }
    }
    assert(saw_panel && saw_button && saw_text);
    assert(wena_native_theme_apply(&ctx));
    assert(equal(ctx.style.window.background, panel));
    nk_free(&ctx);
    puts("native canonical theme and contrast checks passed");
    return 0;
}
