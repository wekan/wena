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
struct nk_input { unsigned int pressed_keys; };

struct nk_context {
    struct nk_input input;
    const char *labels[256];
    int label_count;
    int begin_count;
    int end_count;
    int group_depth;
    int button_count;
    const char *button_to_press;
    const char *edit_text;
    int edit_count;
    const char *combo_item_to_press;
};

#define NK_TEXT_LEFT 0x01
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
int nk_checkbox_label(struct nk_context *context, const char *title, int *active);
int nk_button_label(struct nk_context *context, const char *title);
int nk_group_begin(struct nk_context *context, const char *title,
                   unsigned int flags);
void nk_group_end(struct nk_context *context);

#endif
