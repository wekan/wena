#ifndef NUKLEAR_H_
#define NUKLEAR_H_

struct nk_rect {
    float x;
    float y;
    float w;
    float h;
};

struct nk_context {
    const char *labels[16];
    int label_count;
    int begin_count;
    int end_count;
    int group_depth;
    int button_count;
    int next_button_result;
};

#define NK_TEXT_LEFT 0x01
#define NK_WINDOW_BORDER 0x02
#define NK_WINDOW_NO_SCROLLBAR 0x04
#define NK_DYNAMIC 0

struct nk_rect nk_rect(float x, float y, float w, float h);
int nk_begin(struct nk_context *context, const char *title,
             struct nk_rect bounds, unsigned int flags);
void nk_end(struct nk_context *context);
void nk_layout_row_dynamic(struct nk_context *context, float height, int columns);
void nk_layout_row_begin(struct nk_context *context, int format,
                         float row_height, int columns);
void nk_layout_row_push(struct nk_context *context, float value);
void nk_layout_row_end(struct nk_context *context);
void nk_label(struct nk_context *context, const char *text, int alignment);
int nk_button_label(struct nk_context *context, const char *title);
int nk_group_begin(struct nk_context *context, const char *title,
                   unsigned int flags);
void nk_group_end(struct nk_context *context);

#endif
