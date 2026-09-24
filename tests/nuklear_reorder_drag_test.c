#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_IMPLEMENTATION
#include <nuklear.h>
#include "../client/components/common/reorder_drag.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct Fixture {
    struct nk_context ctx;
    struct nk_user_font font;
    WenaReorderDrag drag;
    struct nk_vec2 points[3];
    unsigned long revision;
    int enabled,hide_source;
} Fixture;
static float width(nk_handle handle,float height,const char *text,int length)
{(void)handle;(void)text;return height*(float)length*0.5f;}
static void frame(Fixture *f,int x,int y,int down,int escape)
{
    struct nk_rect bounds;
    int i;
    const char *ids[]={"first","second","third"};
    nk_clear(&f->ctx);nk_input_begin(&f->ctx);
    nk_input_motion(&f->ctx,x,y);nk_input_button(&f->ctx,NK_BUTTON_LEFT,x,y,down);
    nk_input_key(&f->ctx,NK_KEY_TEXT_RESET_MODE,escape);nk_input_end(&f->ctx);
    wena_reorder_drag_begin(&f->ctx,&f->drag);
    if(nk_begin(&f->ctx,"Reorder",nk_rect(0,0,400,300),NK_WINDOW_BORDER)) {
        for(i=0;i<3;++i) {
            nk_layout_row_dynamic(&f->ctx,30,1);
            bounds=nk_widget_bounds(&f->ctx);
            f->points[i]=nk_vec2(bounds.x+bounds.w/2,bounds.y+bounds.h/2);
            if(i==0&&f->hide_source)nk_label(&f->ctx,"Missing",NK_TEXT_LEFT);
            else (void)wena_reorder_drag_handle(&f->ctx,&f->drag,
                i==2?"group-b":"group-a",f->revision,ids[i],(size_t)i,
                "Move",f->enabled);
        }
    }
    nk_end(&f->ctx);wena_reorder_drag_end(&f->ctx,&f->drag);
}
static void point(Fixture *f,int row,int down,int escape)
{frame(f,(int)f->points[row].x,(int)f->points[row].y,down,escape);}
int main(void)
{
    Fixture f;int i;
    memset(&f,0,sizeof(f));f.font.height=13;f.font.width=width;
    assert(nk_init_default(&f.ctx,&f.font));f.enabled=1;f.revision=1;
    frame(&f,0,0,0,0);
    point(&f,0,1,0);assert(f.drag.active&&!f.drag.pending);
    point(&f,1,1,0);assert(f.drag.active&&!f.drag.pending&&f.drag.moved);
    point(&f,1,0,0);assert(!f.drag.active&&f.drag.pending);
    assert(!strcmp(f.drag.source_id,"first")&&!strcmp(f.drag.target_id,"second"));
    assert(f.drag.revision==1&&f.drag.source_position==0&&f.drag.target_position==1);
    frame(&f,0,0,0,0);assert(!f.drag.pending);
    point(&f,1,1,0);point(&f,0,0,0);
    assert(f.drag.pending&&!strcmp(f.drag.source_id,"second")&&f.drag.target_position==0);
    frame(&f,0,0,0,0);
    point(&f,0,1,0);point(&f,0,0,0);assert(!f.drag.active&&!f.drag.pending);
    point(&f,0,1,0);point(&f,2,0,0);assert(!f.drag.active&&!f.drag.pending);
    point(&f,0,1,0);frame(&f,500,500,0,0);assert(!f.drag.pending);
    point(&f,0,1,0);point(&f,1,1,1);assert(!f.drag.active);
    point(&f,1,0,0);assert(!f.drag.pending);
    point(&f,0,1,0);++f.revision;point(&f,1,0,0);assert(!f.drag.pending);
    point(&f,0,1,0);f.hide_source=1;point(&f,1,0,0);assert(!f.drag.pending);f.hide_source=0;
    f.enabled=0;point(&f,0,1,0);point(&f,1,0,0);assert(!f.drag.active&&!f.drag.pending);
    f.enabled=1;point(&f,0,1,0);f.enabled=0;point(&f,1,0,0);assert(!f.drag.pending);
    for(i=0;i<100;++i)frame(&f,0,0,0,0);
    nk_free(&f.ctx);
    puts("Reusable drag reorder: exact IDs, revisions, threshold, cancel, scope and readonly passed");
    return 0;
}
