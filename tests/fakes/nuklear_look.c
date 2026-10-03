/* The part of the fake Nuklear that WeKan's board look uses: colors, style
 * stacks, layout space, mouse queries and drawing - all no-ops - and the
 * unnamed buttons it draws over its icons and links. Linked with
 * tests/fakes/nuklear.c, or with a test's own core stubs. */
#include "nuklear.h"
#include "../../client/components/common/wekan_look.h"
#include <stddef.h>
#include <string.h>

struct nk_color nk_rgb(int r, int g, int b) { return nk_rgba(r, g, b, 255); }
struct nk_color nk_rgba(int r, int g, int b, int a)
{
    struct nk_color color;
    color.r = (unsigned char)r; color.g = (unsigned char)g; color.b = (unsigned char)b; color.a = (unsigned char)a;
    return color;
}
struct nk_style_item nk_style_item_color(struct nk_color color)
{
    struct nk_style_item item; item.type = NK_STYLE_ITEM_COLOR; item.data.color = color; return item;
}
int nk_style_push_style_item(struct nk_context *c, struct nk_style_item *a, struct nk_style_item b) { (void)c; (void)a; (void)b; return 1; }
int nk_style_pop_style_item(struct nk_context *c) { (void)c; return 1; }
int nk_style_push_vec2(struct nk_context *c, struct nk_vec2 *a, struct nk_vec2 b) { (void)c; (void)a; (void)b; return 1; }
int nk_style_pop_vec2(struct nk_context *c) { (void)c; return 1; }
int nk_style_push_color(struct nk_context *c, struct nk_color *a, struct nk_color b) { (void)c; (void)a; (void)b; return 1; }
int nk_style_pop_color(struct nk_context *c) { (void)c; return 1; }
int nk_style_push_font(struct nk_context *c, const struct nk_user_font *f) { (void)c; (void)f; return 1; }
int nk_style_pop_font(struct nk_context *c) { (void)c; return 1; }
void nk_layout_space_begin(struct nk_context *c, int f, float h, int n) { (void)c; (void)f; (void)h; (void)n; }
void nk_layout_space_push(struct nk_context *c, struct nk_rect r) { (void)c; (void)r; }
void nk_layout_space_end(struct nk_context *c) { (void)c; }
struct nk_rect nk_layout_space_bounds(struct nk_context *c) { (void)c; return nk_rect(0, 0, 0, 0); }
struct nk_vec2 nk_layout_space_to_screen(struct nk_context *c, struct nk_vec2 v) { (void)c; return v; }
struct nk_rect nk_widget_bounds(struct nk_context *c) { (void)c; return nk_rect(0, 0, 0, 0); }
enum nk_widget_layout_states nk_widget(struct nk_rect *r, const struct nk_context *c) { (void)c; *r = nk_rect(0, 0, 0, 0); return NK_WIDGET_INVALID; }
int nk_widget_is_hovered(struct nk_context *c) { (void)c; return 0; }
void nk_spacing(struct nk_context *c, int n) { (void)c; (void)n; }
int nk_input_is_mouse_hovering_rect(const struct nk_input *i, struct nk_rect r) { (void)i; (void)r; return 0; }
int nk_input_is_mouse_pressed(const struct nk_input *i, enum nk_buttons b) { (void)i; (void)b; return 0; }
int nk_input_is_mouse_released(const struct nk_input *i, enum nk_buttons b) { (void)i; (void)b; return 0; }
int nk_input_is_mouse_down(const struct nk_input *i, enum nk_buttons b) { (void)i; (void)b; return 0; }
int nk_input_has_mouse_click_in_rect(const struct nk_input *i, enum nk_buttons b, struct nk_rect r) { (void)i; (void)b; (void)r; return 0; }
/* An unnamed button is the control the look module just named: a test
 * presses it by that name ("Add Card to Top of List", "List Actions"). */
int nk_button_text_styled(struct nk_context *context, const struct nk_style_button *style, const char *text, int length)
{
    size_t count;
    const WenaUiControl *controls;
    (void)style; (void)text; (void)length;
    controls = wena_ui_controls(&count);
    return count > 0 ? nk_button_label(context, controls[count - 1].name) : nk_button_label(context, "");
}
int nk_button_label_styled(struct nk_context *context, const struct nk_style_button *style, const char *title)
{
    (void)style;
    return nk_button_label(context, title);
}
void nk_label_colored(struct nk_context *context, const char *text, nk_flags align, struct nk_color color)
{
    (void)color;
    nk_label(context, text, (int)align);
}
void nk_label_colored_wrap(struct nk_context *context, const char *text, struct nk_color color)
{
    (void)color;
    nk_label(context, text, NK_TEXT_LEFT);
}
void nk_tooltip(struct nk_context *c, const char *t) { (void)c; (void)t; }
void nk_window_set_focus(struct nk_context *c, const char *n) { (void)c; (void)n; }
struct nk_command_buffer *nk_window_get_canvas(struct nk_context *context) { return &context->canvas; }
void nk_fill_rect(struct nk_command_buffer *b, struct nk_rect r, float f, struct nk_color c) { (void)r; (void)f; (void)c; ++b->commands; }
void nk_stroke_rect(struct nk_command_buffer *b, struct nk_rect r, float f, float t, struct nk_color c) { (void)r; (void)f; (void)t; (void)c; ++b->commands; }
void nk_stroke_line(struct nk_command_buffer *b, float x0, float y0, float x1, float y1, float t, struct nk_color c) { (void)x0; (void)y0; (void)x1; (void)y1; (void)t; (void)c; ++b->commands; }
void nk_fill_circle(struct nk_command_buffer *b, struct nk_rect r, struct nk_color c) { (void)r; (void)c; ++b->commands; }
void nk_stroke_circle(struct nk_command_buffer *b, struct nk_rect r, float t, struct nk_color c) { (void)r; (void)t; (void)c; ++b->commands; }
void nk_fill_triangle(struct nk_command_buffer *b, float x0, float y0, float x1, float y1, float x2, float y2, struct nk_color c) { (void)x0; (void)y0; (void)x1; (void)y1; (void)x2; (void)y2; (void)c; ++b->commands; }
void nk_stroke_polyline(struct nk_command_buffer *b, float *p, int n, float t, struct nk_color c) { (void)p; (void)n; (void)t; (void)c; ++b->commands; }
void nk_fill_polygon(struct nk_command_buffer *b, float *p, int n, struct nk_color c) { (void)p; (void)n; (void)c; ++b->commands; }
void nk_fill_arc(struct nk_command_buffer *b, float x, float y, float r, float a0, float a1, struct nk_color c) { (void)x; (void)y; (void)r; (void)a0; (void)a1; (void)c; ++b->commands; }
void nk_stroke_arc(struct nk_command_buffer *b, float x, float y, float r, float a0, float a1, float t, struct nk_color c) { (void)x; (void)y; (void)r; (void)a0; (void)a1; (void)t; (void)c; ++b->commands; }
/* Text the look module draws itself ("+ Add Card", header links) is
 * recorded through its control name; the fake has no fonts to draw with. */
void nk_draw_text(struct nk_command_buffer *b, struct nk_rect r, const char *t, int n,
                  const struct nk_user_font *f, struct nk_color bg, struct nk_color fg)
{ (void)r; (void)t; (void)n; (void)f; (void)bg; (void)fg; ++b->commands; }
