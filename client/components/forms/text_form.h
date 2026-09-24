#ifndef WENA_TEXT_FORM_H
#define WENA_TEXT_FORM_H
struct nk_context;
#define WENA_TEXT_FORM_SAVE 1u
#define WENA_TEXT_FORM_CANCEL 2u
/* Shared focus boundary. Escape wins over a focused editor's Enter commit. */
unsigned int wena_text_form_keys(struct nk_context *context,unsigned int edit_result);
/* Caller owns draft storage and validation. Capacity includes NUL and should
 * retain an overflow scalar using WENA_NATIVE_EDIT_CAPACITY where appropriate.
 * This component reports intent only; it never performs persistence. */
unsigned int wena_text_form_render(struct nk_context *context,char *text,int *length,
    int capacity,int error);
/* Multiline mode inserts newlines on Enter; Save requires a button click. */
unsigned int wena_text_form_render_mode(struct nk_context *context,char *text,
    int *length,int capacity,int error,int multiline);
#endif
