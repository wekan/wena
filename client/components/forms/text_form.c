#include "text_form.h"
#include "../../../imports/ui/page_contract.h"
#include <nuklear.h>
unsigned int wena_text_form_keys(struct nk_context *context,unsigned int edit_result)
{
    if (!context || !nk_window_has_focus(context)) return 0;
    if (nk_input_is_key_pressed(&context->input,NK_KEY_TEXT_RESET_MODE)) return WENA_TEXT_FORM_CANCEL;
    return (edit_result & NK_EDIT_COMMITED) ? WENA_TEXT_FORM_SAVE : 0;
}
unsigned int wena_text_form_render(struct nk_context *context,char *text,int *length,
    int capacity,int error)
{
    unsigned int result,edit;
    if (!context || !text || !length || capacity<2 || *length<0 || *length>=capacity) return 0;
    result=wena_text_form_keys(context,0);
    if (result & WENA_TEXT_FORM_CANCEL) return result;
    nk_layout_row_dynamic(context,30,1);
    edit=nk_edit_string(context,NK_EDIT_FIELD|NK_EDIT_SIG_ENTER,text,length,capacity,nk_filter_default);
    result=wena_text_form_keys(context,edit);
    nk_layout_row_dynamic(context,28,2);
    if (nk_button_label(context,wena_ui_control_text(WENA_UI_SAVE))) result|=WENA_TEXT_FORM_SAVE;
    if (nk_button_label(context,wena_ui_control_text(WENA_UI_CANCEL))) result=WENA_TEXT_FORM_CANCEL;
    if (*length<0 || *length>=capacity) return result & WENA_TEXT_FORM_CANCEL;
    text[*length]=0;
    if (error) {
        nk_layout_row_dynamic(context,48,1);
        nk_label_wrap(context,wena_ui_text(WENA_UI_TEXT_OPERATION_FAILED));
    }
    return result;
}
