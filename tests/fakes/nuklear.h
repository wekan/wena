#ifndef NUKLEAR_H_
#define NUKLEAR_H_

struct nk_vec2 { float x; float y; };

struct nk_rect {
    float x;
    float y;
    float w;
    float h;
};

enum nk_keys { NK_KEY_ENTER, NK_KEY_TEXT_RESET_MODE };
enum nk_buttons { NK_BUTTON_LEFT, NK_BUTTON_MIDDLE, NK_BUTTON_RIGHT };
enum nk_widget_layout_states { NK_WIDGET_INVALID, NK_WIDGET_VALID };
struct nk_mouse_button { int down; unsigned int clicked; struct nk_vec2 clicked_pos; };
struct nk_mouse { struct nk_vec2 pos; struct nk_mouse_button buttons[3]; struct nk_vec2 scroll_delta; };
struct nk_input { unsigned int pressed_keys; struct nk_mouse mouse; };

/* What WeKan's board look (client/components/common/wekan_look.c) reads of
 * Nuklear: colors, fonts, style fields and the current panel's layout. The
 * fake draws nothing; its unnamed buttons answer a click aimed at the
 * control just named (see nk_button_text_styled). */
typedef unsigned int nk_flags;
struct nk_color { unsigned char r, g, b, a; };
enum nk_style_item_type { NK_STYLE_ITEM_COLOR };
union nk_style_item_data { struct nk_color color; };
struct nk_style_item { enum nk_style_item_type type; union nk_style_item_data data; };
struct nk_user_font { void *userdata; float height; float (*width)(void *, float, const char *, int); };
struct nk_style_button {
    struct nk_style_item normal, hover, active;
    struct nk_color border_color, text_normal, text_hover, text_active;
    float border, rounding;
    struct nk_vec2 padding;
};
struct nk_style_window {
    struct nk_style_item fixed_background;
    struct nk_vec2 padding, group_padding, popup_padding, spacing, scrollbar_size;
    float group_border;
    struct nk_color border_color;
};
struct nk_style_edit { struct nk_style_item normal, hover, active; float border; };
struct nk_style { const struct nk_user_font *font; struct nk_style_window window; struct nk_style_button button;
                  struct nk_style_edit edit; };
struct nk_row_layout { float height; };
struct nk_panel { float at_y; struct nk_row_layout row; struct nk_rect bounds, clip; };
struct nk_window { struct nk_panel *layout; };
struct nk_command_buffer { int commands; };

struct nk_context {
    struct nk_input input;
    struct nk_style style;
    struct nk_window *current;
    struct nk_window window;
    struct nk_panel panel;
    struct nk_command_buffer canvas;
    const char *labels[256];
    int label_count;
    int begin_count;
    int end_count;
    int group_depth;
    int button_count;
    const char *button_to_press;
    /* Style vec2 pushes still open, and those hiding the scrollbar. */
    int vec2_depth, scrollbar_hidden;
    const char *edit_text;
    int edit_commit;          /* the next edit reports Enter */
    int edit_count;
    const char *combo_item_to_press;
};

#define NK_TEXT_LEFT 0x01
#define NK_TEXT_CENTERED 0x12
#define NK_TEXT_RIGHT 0x14
#define NK_WINDOW_BORDER 0x02
#define NK_WINDOW_NO_SCROLLBAR 0x04
#define NK_DYNAMIC 0
#define NK_STATIC 1
#define NK_EDIT_BOX 8
#define NK_EDIT_READ_ONLY 16
#define NK_EDIT_FIELD 1
#define NK_EDIT_SIG_ENTER 4u
#define NK_EDIT_COMMITED 16u
typedef unsigned int nk_rune;
struct nk_text_edit;
typedef int (*nk_plugin_filter)(const struct nk_text_edit *, nk_rune);
int nk_filter_default(const struct nk_text_edit *edit, nk_rune rune);
int nk_filter_decimal(const struct nk_text_edit *edit, nk_rune rune);
unsigned int nk_edit_string(struct nk_context *context, unsigned int flags,
    char *buffer, int *length, int max, nk_plugin_filter filter);

struct nk_vec2 nk_vec2(float x, float y);
int nk_combo_begin_label(struct nk_context *, const char *, struct nk_vec2);
int nk_combo_item_label(struct nk_context *, const char *, int);
void nk_combo_end(struct nk_context *);
int nk_combo_callback(struct nk_context *, void (*)(void *, int, const char **),
    void *, int, int, int, struct nk_vec2);

struct nk_rect nk_rect(float x, float y, float w, float h);
int nk_begin(struct nk_context *context, const char *title,
             struct nk_rect bounds, unsigned int flags);
int nk_begin_titled(struct nk_context *context, const char *name,
    const char *title, struct nk_rect bounds, unsigned int flags);
void nk_end(struct nk_context *context);
int nk_window_has_focus(const struct nk_context *context);
int nk_input_is_key_pressed(const struct nk_input *input, enum nk_keys key);
void nk_layout_row_dynamic(struct nk_context *context, float height, int columns);
void nk_layout_row_begin(struct nk_context *context, int format,
                         float row_height, int columns);
void nk_layout_row_push(struct nk_context *context, float value);
void nk_layout_row_end(struct nk_context *context);
void nk_label(struct nk_context *context, const char *text, int alignment);
/* Fake records semantic text; real Nuklear suites verify actual wrapping. */
#define nk_label_wrap(context, text) nk_label(context, text, NK_TEXT_LEFT)
int nk_property_int(struct nk_context*,const char*,int,int*,int,int,float);
int nk_checkbox_label(struct nk_context *context, const char *title, int *active);
int nk_button_label(struct nk_context *context, const char *title);
int nk_group_begin(struct nk_context *context, const char *title,
                   unsigned int flags);
void nk_group_end(struct nk_context *context);
int nk_group_begin_titled(struct nk_context *context, const char *name, const char *title, unsigned int flags);
struct nk_image { void *ptr; };
struct nk_image nk_image_ptr(void *ptr);
void nk_image(struct nk_context *context, struct nk_image image);
void nk_layout_row(struct nk_context *context, int format, float height, int columns, const float *ratio);
void nk_layout_row_static(struct nk_context *context, float height, int item_width, int columns);

struct nk_color nk_rgb(int r, int g, int b);
struct nk_color nk_rgba(int r, int g, int b, int a);
struct nk_style_item nk_style_item_color(struct nk_color color);
int nk_style_push_style_item(struct nk_context *, struct nk_style_item *, struct nk_style_item);
int nk_style_pop_style_item(struct nk_context *);
int nk_style_push_vec2(struct nk_context *, struct nk_vec2 *, struct nk_vec2);
int nk_style_pop_vec2(struct nk_context *);
int nk_style_push_color(struct nk_context *, struct nk_color *, struct nk_color);
int nk_style_pop_color(struct nk_context *);
int nk_style_push_font(struct nk_context *, const struct nk_user_font *);
int nk_style_push_float(struct nk_context *, float *, float);
int nk_style_pop_float(struct nk_context *);
void nk_edit_focus(struct nk_context *, unsigned int flags);
int nk_style_pop_font(struct nk_context *);
void nk_layout_space_begin(struct nk_context *, int format, float height, int count);
void nk_layout_space_push(struct nk_context *, struct nk_rect);
void nk_layout_space_end(struct nk_context *);
struct nk_rect nk_layout_space_bounds(struct nk_context *);
struct nk_vec2 nk_layout_space_to_screen(struct nk_context *, struct nk_vec2);
struct nk_rect nk_widget_bounds(struct nk_context *);
enum nk_widget_layout_states nk_widget(struct nk_rect *, const struct nk_context *);
int nk_widget_is_hovered(struct nk_context *);
void nk_spacing(struct nk_context *, int columns);
void nk_spacer(struct nk_context *);
int nk_input_is_mouse_hovering_rect(const struct nk_input *, struct nk_rect);
int nk_input_mouse_clicked(const struct nk_input *, enum nk_buttons, struct nk_rect);
int nk_input_is_mouse_pressed(const struct nk_input *, enum nk_buttons);
int nk_input_is_mouse_released(const struct nk_input *, enum nk_buttons);
int nk_input_is_mouse_down(const struct nk_input *, enum nk_buttons);
int nk_input_has_mouse_click_in_rect(const struct nk_input *, enum nk_buttons, struct nk_rect);
int nk_button_text_styled(struct nk_context *, const struct nk_style_button *, const char *, int);
int nk_button_label_styled(struct nk_context *, const struct nk_style_button *, const char *);
void nk_label_colored(struct nk_context *, const char *, nk_flags, struct nk_color);
void nk_label_colored_wrap(struct nk_context *, const char *, struct nk_color);
void nk_tooltip(struct nk_context *, const char *);
void nk_window_set_focus(struct nk_context *, const char *);
struct nk_command_buffer *nk_window_get_canvas(struct nk_context *);
void nk_fill_rect(struct nk_command_buffer *, struct nk_rect, float, struct nk_color);
void nk_stroke_rect(struct nk_command_buffer *, struct nk_rect, float, float, struct nk_color);
void nk_stroke_line(struct nk_command_buffer *, float, float, float, float, float, struct nk_color);
void nk_fill_circle(struct nk_command_buffer *, struct nk_rect, struct nk_color);
void nk_stroke_circle(struct nk_command_buffer *, struct nk_rect, float, struct nk_color);
void nk_fill_triangle(struct nk_command_buffer *, float, float, float, float, float, float, struct nk_color);
void nk_stroke_polyline(struct nk_command_buffer *, float *, int, float, struct nk_color);
void nk_fill_polygon(struct nk_command_buffer *, float *, int, struct nk_color);
void nk_fill_arc(struct nk_command_buffer *, float, float, float, float, float, struct nk_color);
void nk_stroke_arc(struct nk_command_buffer *, float, float, float, float, float, float, struct nk_color);
void nk_draw_text(struct nk_command_buffer *, struct nk_rect, const char *, int,
                  const struct nk_user_font *, struct nk_color, struct nk_color);

#endif
