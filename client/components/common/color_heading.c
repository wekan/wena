#include "color_heading.h"
#include "../../../models/color.h"
#include <nuklear.h>

static void paint_heading(struct nk_context *context,const char *title,
    struct nk_color background,struct nk_color foreground,int wrap)
{
    struct nk_color previous;
    previous=context->style.window.background;
    context->style.window.background=background;
    nk_fill_rect(nk_window_get_canvas(context),nk_widget_bounds(context),0,background);
    if(wrap)nk_label_colored_wrap(context,title,foreground);
    else nk_label_colored(context,title,NK_TEXT_LEFT,foreground);
    context->style.window.background=previous;
}

void wena_color_heading(struct nk_context *context,const char *title,
    const char *color,int wrap)
{
    unsigned char rgb[3],contrast[3];struct nk_color background,foreground;
    if(!context||!title)return;
    if(!color||!color[0]||!wena_color_rgb(color,rgb)||!wena_color_foreground(color,contrast)){
        if(wrap)nk_label_wrap(context,title);else nk_label(context,title,NK_TEXT_LEFT);
        return;
    }
    background=nk_rgb(rgb[0],rgb[1],rgb[2]);foreground=nk_rgb(contrast[0],contrast[1],contrast[2]);
    paint_heading(context,title,background,foreground,wrap);
}

void wena_selection_heading(struct nk_context *context,const char *title,int selected,int wrap)
{
    const struct nk_style_selectable *style;struct nk_color previous;
    if(!context||!title)return;
    style=&context->style.selectable;
    if(!selected){
        if(wrap)nk_label_wrap(context,title);else nk_label(context,title,NK_TEXT_LEFT);
        return;
    }
    /* Shared theme supplies the selected foreground/background pair. Preserve
     * texture-backed themes rather than interpreting an image handle as RGB. */
    if(style->normal_active.type==NK_STYLE_ITEM_COLOR)
        paint_heading(context,title,style->normal_active.data.color,style->text_normal_active,wrap);
    else{
        if(style->normal_active.type==NK_STYLE_ITEM_IMAGE)
            nk_draw_image(nk_window_get_canvas(context),nk_widget_bounds(context),
                &style->normal_active.data.image,nk_rgba(255,255,255,255));
        else if(style->normal_active.type==NK_STYLE_ITEM_NINE_SLICE)
            nk_draw_nine_slice(nk_window_get_canvas(context),nk_widget_bounds(context),
                &style->normal_active.data.slice,nk_rgba(255,255,255,255));
        previous=context->style.window.background;
        context->style.window.background=nk_rgba(0,0,0,0);
        if(wrap)nk_label_colored_wrap(context,title,style->text_normal_active);
        else nk_label_colored(context,title,NK_TEXT_LEFT,style->text_normal_active);
        context->style.window.background=previous;
    }
}
