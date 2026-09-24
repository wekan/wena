#include "color_heading.h"
#include "../../../models/color.h"
#include <nuklear.h>

void wena_color_heading(struct nk_context *context,const char *title,
    const char *color,int wrap)
{
    unsigned char rgb[3],contrast[3];struct nk_color background,foreground,previous;
    if(!context||!title)return;
    if(!color||!color[0]||!wena_color_rgb(color,rgb)||!wena_color_foreground(color,contrast)){
        if(wrap)nk_label_wrap(context,title);else nk_label(context,title,NK_TEXT_LEFT);
        return;
    }
    background=nk_rgb(rgb[0],rgb[1],rgb[2]);foreground=nk_rgb(contrast[0],contrast[1],contrast[2]);
    previous=context->style.window.background;
    context->style.window.background=background;
    nk_fill_rect(nk_window_get_canvas(context),nk_widget_bounds(context),0,background);
    if(wrap)nk_label_colored_wrap(context,title,foreground);
    else nk_label_colored(context,title,NK_TEXT_LEFT,foreground);
    context->style.window.background=previous;
}
