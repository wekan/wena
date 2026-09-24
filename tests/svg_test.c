/* SPDX-License-Identifier: MIT */
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_IMPLEMENTATION
#include <nuklear.h>
#include "../client/platform/svg.h"
#include "../client/components/boards/board_header.h"
#include "../client/platform/theme.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static float width(nk_handle handle,float height,const char *text,int length)
{ (void)handle;(void)text;return height*(float)length*0.5f; }
static size_t commands(struct nk_context *ctx)
{
    const struct nk_command *c;size_t count;count=0;
    nk_foreach(c,ctx) { ++count; }return count;
}
int main(void)
{
    struct nk_context ctx;
    struct nk_user_font font;
    struct nk_command_buffer *canvas;
    struct nk_color ink;
    struct nk_rect box;
    const struct nk_command *c;
    const struct nk_command_rect *rect;
    const struct nk_command_text *text;
    WenaSvgAsset asset;
    WenaSvgShape shapes[4];
    WenaBoard board;
    size_t before;
    int scale, rects, saw_text;
    memset(&font,0,sizeof(font));font.height=14;font.width=width;
    assert(nk_init_default(&ctx,&font));assert(wena_native_theme_apply(&ctx));
    ink=nk_rgb(24,91,173);
    assert(wena_svg_board()->count==4);
    assert(sizeof(shapes)+sizeof(asset)<1024u);
    for(scale=1;scale<=4;scale*=2){
        nk_clear(&ctx);
        if(nk_begin(&ctx,"Vectors",nk_rect(0,0,400,300),0)){
            canvas=nk_window_get_canvas(&ctx);
            /* Non-square destination letterboxes rather than stretching. */
            box=nk_rect(20,40,48.0f*(float)scale,24.0f*(float)scale);
            assert(wena_svg_draw(canvas,wena_svg_board(),&box,&ink));
        }
        nk_end(&ctx);rects=0;
        nk_foreach(c,&ctx){
            if(c->type==NK_COMMAND_RECT){
                rect=(const struct nk_command_rect*)c;
                if(rect->color.r==ink.r&&rect->color.g==ink.g&&rect->color.b==ink.b){
                    assert(rect->x==20+14*scale&&rect->y==40+3*scale);
                    assert(rect->w==20*scale&&rect->h==18*scale);++rects;
                }
            }
            assert(c->type!=NK_COMMAND_IMAGE);
        }
        assert(rects==1);
    }
    /* Every advertised primitive converts to native commands, not textures. */
    nk_clear(&ctx);
    assert(nk_begin(&ctx,"Vectors",nk_rect(0,0,400,300),0));
    canvas=nk_window_get_canvas(&ctx);box=nk_rect(20,40,24,24);
    asset=*wena_svg_board();memcpy(shapes,asset.shapes,sizeof(shapes));asset.shapes=shapes;
    asset.count=2;
    shapes[0].kind=2;shapes[0].geometry[0]=8;shapes[0].geometry[1]=8;
    shapes[0].geometry[2]=8;shapes[0].geometry[3]=8;shapes[0].geometry[4]=0;
    shapes[0].fill=1193046L;shapes[0].stroke=-1L;
    shapes[1].kind=3;shapes[1].geometry[0]=2;shapes[1].geometry[1]=3;
    shapes[1].geometry[2]=20;shapes[1].geometry[3]=21;shapes[1].geometry[4]=0;
    shapes[1].fill=-1L;shapes[1].stroke=-2L;
    assert(wena_svg_draw(canvas,&asset,&box,&ink));nk_end(&ctx);
    rects=0;saw_text=0;
    nk_foreach(c,&ctx){
        if(c->type==NK_COMMAND_CIRCLE_FILLED){
            const struct nk_command_circle_filled *circle;
            circle=(const struct nk_command_circle_filled*)c;
            assert(circle->x==28&&circle->y==48&&circle->w==8&&circle->h==8);
            assert(circle->color.r==18&&circle->color.g==52&&circle->color.b==86);
            ++rects;
        }
        if(c->type==NK_COMMAND_LINE){
            const struct nk_command_line *line;
            line=(const struct nk_command_line*)c;
            assert(line->begin.x==22&&line->begin.y==43&&line->end.x==40&&line->end.y==61);
            ++saw_text;
        }
    }
    assert(rects==1&&saw_text==1);
    /* Entire assets reject before emitting even their first valid primitive. */
    nk_clear(&ctx);
    assert(nk_begin(&ctx,"Vectors",nk_rect(0,0,400,300),0));
    canvas=nk_window_get_canvas(&ctx);box=nk_rect(20,40,24,24);
    asset=*wena_svg_board();memcpy(shapes,asset.shapes,sizeof(shapes));asset.shapes=shapes;
    before=canvas->end;shapes[3].kind=99;
    assert(!wena_svg_draw(canvas,&asset,&box,&ink)&&canvas->end==before);
    shapes[3]=wena_svg_board()->shapes[3];shapes[3].geometry[2]=-1;
    assert(!wena_svg_draw(canvas,&asset,&box,&ink)&&canvas->end==before);
    shapes[3]=wena_svg_board()->shapes[3];shapes[3].stroke=16777216L;
    assert(!wena_svg_draw(canvas,&asset,&box,&ink)&&canvas->end==before);
    shapes[3]=wena_svg_board()->shapes[3];asset.width=0;
    assert(!wena_svg_draw(canvas,&asset,&box,&ink)&&canvas->end==before);
    asset.width=24;asset.count=129;
    assert(!wena_svg_draw(canvas,&asset,&box,&ink)&&canvas->end==before);
    asset.count=4;box.w=-1;
    assert(!wena_svg_draw(canvas,&asset,&box,&ink)&&canvas->end==before);
    box.w=24;assert(!wena_svg_draw(NULL,&asset,&box,&ink));
    assert(!wena_svg_draw(canvas,NULL,&box,&ink));
    assert(!wena_svg_draw(canvas,&asset,NULL,&ink));
    assert(!wena_svg_draw(canvas,&asset,&box,NULL));
    nk_end(&ctx);
    /* Real native title hook keeps live text, with vectors but no images. */
    assert(wena_board_init(&board,"b","Board title",0));
    nk_clear(&ctx);wena_board_header_set_title_renderer(wena_svg_board_title);
    if(nk_begin(&ctx,"Vectors",nk_rect(0,0,400,300),0))
        (void)wena_board_header_render(&ctx,&board);
    nk_end(&ctx);saw_text=0;assert(commands(&ctx)>4);
    nk_foreach(c,&ctx){
        assert(c->type!=NK_COMMAND_IMAGE);
        if(c->type==NK_COMMAND_TEXT){
            text=(const struct nk_command_text*)c;
            if(text->length==11&&!memcmp(text->string,"Board title",11))saw_text=1;
        }
    }
    assert(saw_text);wena_board_header_set_title_renderer(NULL);nk_free(&ctx);
    puts("SVG native vectors: 1x/2x/4x, aspect, themed ink, invalid atomicity and live header passed");
    return 0;
}
