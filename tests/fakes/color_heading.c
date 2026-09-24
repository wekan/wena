#include "../../client/components/common/color_heading.h"
#include "nuklear.h"
void wena_color_heading(struct nk_context *context,const char *title,const char *color,int wrap)
{
    (void)color;
    if(wrap)nk_label_wrap(context,title);else nk_label(context,title,NK_TEXT_LEFT);
}

void wena_selection_heading(struct nk_context *context,const char *title,int selected,int wrap)
{
    (void)selected;
    if(wrap)nk_label_wrap(context,title);else nk_label(context,title,NK_TEXT_LEFT);
}
