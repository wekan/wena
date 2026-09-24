#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_IMPLEMENTATION
#include <nuklear.h>
#include "../client/components/common/paginated_table.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static size_t rows[256], calls;
static const char *titles[] = {"First", "Second", "Third", "Fourth", "Fifth"};
static unsigned int row(struct nk_context *ctx, void *data, size_t index)
{
    const char *const *labels;
    labels = (const char *const *)data;
    assert(calls < 256); rows[calls++] = index;
    nk_label(ctx, "Record", NK_TEXT_LEFT);
    return nk_button_label(ctx, labels[index]) ? 8u : 0u;
}
static unsigned int virtual_row(struct nk_context *ctx, void *data, size_t index)
{
    (void)data;
    assert(calls < 256); rows[calls++] = index;
    nk_label(ctx, "Virtual record", NK_TEXT_LEFT); return 0;
}
static float width(nk_handle h, float height, const char *text, int length)
{ (void)h; (void)text; return height * (float)length * 0.5f; }
static WenaTableResult draw(struct nk_context *ctx, WenaTableState *state,
    WenaTableView *view)
{
    WenaTableResult result;
    memset(&result, 0, sizeof(result)); calls = 0;
    if (nk_begin(ctx, "Table", nk_rect(0,0,600,500), NK_WINDOW_BORDER))
        result = wena_table_render(ctx, state, view);
    nk_end(ctx); return result;
}
static WenaTableResult frame(struct nk_context *ctx, WenaTableState *state,
    WenaTableView *view)
{
    nk_clear(ctx); nk_input_begin(ctx); nk_input_end(ctx);
    return draw(ctx, state, view);
}
static struct nk_vec2 center(struct nk_context *ctx, const char *label)
{
    const struct nk_command *command;
    nk_foreach(command, ctx) if (command->type == NK_COMMAND_TEXT) {
        const struct nk_command_text *text;
        text = (const struct nk_command_text *)command;
        if ((size_t)text->length == strlen(label) && !memcmp(text->string,label,(size_t)text->length))
            return nk_vec2(text->x + text->w * 0.5f, text->y + text->h * 0.5f);
    }
    fprintf(stderr,"Missing table text: %s\n",label); assert(0); return nk_vec2(0,0);
}
static WenaTableResult click(struct nk_context *ctx, WenaTableState *state,
    WenaTableView *view, const char *label)
{
    struct nk_vec2 point; int down; WenaTableResult result, pending;
    point = center(ctx,label); memset(&result,0,sizeof(result));
    for (down=1; down>=0; --down) {
        nk_clear(ctx); nk_input_begin(ctx);
        nk_input_motion(ctx,(int)point.x,(int)point.y);
        nk_input_button(ctx,NK_BUTTON_LEFT,(int)point.x,(int)point.y,down);
        nk_input_end(ctx); pending = draw(ctx,state,view);
        if (pending.action || pending.page_changed) result = pending;
    }
    return result;
}
int main(void)
{
    struct nk_context ctx; struct nk_user_font font;
    WenaTableState state, other; WenaTableView view; WenaTableResult result;
    const char *headings[] = {"Kind","Title"}; size_t n;
    memset(&font,0,sizeof(font));font.height=13;font.width=width;
    assert(nk_init_default(&ctx,&font));
    assert(wena_table_init(&state,2)); assert(wena_table_init(&other,10));
    assert(!wena_table_init(NULL,2));assert(!wena_table_init(&state,0));
    assert(!wena_table_init(&state,257));assert(state.page_size==2);
    memset(&view,0,sizeof(view));view.row_count=5;view.column_count=2;
    view.row_height=24;view.headings=headings;view.render_row=row;view.context=(void*)titles;
    result=frame(&ctx,&state,&view);assert(result.valid&&!result.action&&calls==2&&rows[0]==0&&rows[1]==1);
    result=click(&ctx,&state,&view,"Next Page");assert(result.page_changed&&state.page==1&&calls==2&&rows[0]==2&&rows[1]==3);
    result=click(&ctx,&state,&view,"Fourth");assert(result.action==8&&result.row==3&&!result.page_changed);
    result=click(&ctx,&state,&view,"Next Page");assert(state.page==2&&calls==1&&rows[0]==4);
    result=click(&ctx,&state,&view,"Next Page");assert(!result.action&&!result.page_changed&&state.page==2);
    result=click(&ctx,&state,&view,"Previous Page");assert(result.page_changed&&state.page==1);
    view.row_count=1;result=frame(&ctx,&state,&view);assert(result.page_changed&&!state.page&&calls==1);
    view.row_count=0;result=frame(&ctx,&state,&view);assert(result.valid&&!calls);center(&ctx,"No items.");
    view.error_text="Load failed";view.row_count=5;view.render_row=NULL;
    result=frame(&ctx,&state,&view);assert(result.valid&&!calls&&!result.action);center(&ctx,"Load failed");
    view.error_text=NULL;result=frame(&ctx,&state,&view);assert(!result.valid&&!calls);
    view.render_row=row;view.row_count=5;result=frame(&ctx,&other,&view);assert(result.valid&&calls==5&&other.page==0);
    /* A different row adapter, the same template, and maximal size_t bounds. */
    view.headings=NULL;view.render_row=virtual_row;view.context=NULL;view.column_count=1;
    view.row_count=(size_t)-1;state.page=(size_t)-1;
    result=frame(&ctx,&state,&view);assert(result.valid&&result.page_changed&&calls==1&&rows[0]==(size_t)-2);
    assert(wena_table_init(&state,1));state.page=(size_t)-1;
    result=frame(&ctx,&state,&view);assert(result.valid&&calls==1&&rows[0]==(size_t)-2);
    assert(wena_table_init(&state,256));view.row_count=256;
    result=frame(&ctx,&state,&view);assert(result.valid&&calls==256);
    for(n=0;n<256;++n)assert(rows[n]==n);
    view.column_count=17;result=frame(&ctx,&state,&view);assert(!result.valid&&!calls);
    view.column_count=1;view.row_height=0;result=frame(&ctx,&state,&view);assert(!result.valid&&!calls);
    assert(!wena_table_render(NULL,&state,&view).valid);
    nk_free(&ctx);puts("Reusable table navigation, absolute row intents, adapters, errors and size_t bounds passed");return 0;
}
