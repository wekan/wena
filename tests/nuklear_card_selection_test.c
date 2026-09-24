#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_IMPLEMENTATION
#include <nuklear.h>
#include "../client/features/card_selection_panel.h"
#include "../client/components/cards/card_body.h"
#include "../client/components/boards/board_layout.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static float text_width(nk_handle handle, float height,
                        const char *text, int length)
{
    (void)handle; (void)text;
    return height * (float)length * 0.5f;
}

static void render(struct nk_context *ctx, WenaCardSelectionPanel *state,
                   WenaBoardLayout *layout)
{
    const struct nk_command *command;
    if (state->visible)
        assert(wena_card_selection_panel_render(ctx,state,layout->cards,layout->card_count,layout->board->id,640,480));
    assert(ctx->current == NULL);
    nk_foreach(command, ctx) { (void)command; }
}

/* Locate rendered text, avoiding assumptions about font-specific button widths. */
static struct nk_vec2 label_center(struct nk_context *ctx, const char *label)
{
    const struct nk_command *command;
    const struct nk_command_text *text;
    nk_foreach(command, ctx) {
        if (command->type != NK_COMMAND_TEXT) continue;
        text = (const struct nk_command_text *)command;
        if ((size_t)text->length == strlen(label) &&
            memcmp(text->string, label, (size_t)text->length) == 0)
            return nk_vec2((float)text->x + (float)text->w * 0.5f,
                           (float)text->y + (float)text->h * 0.5f);
    }
    fprintf(stderr, "missing rendered control: %s\n", label);
    assert(0);
    return nk_vec2(0.0f, 0.0f);
}

static void click_at(struct nk_context *ctx, WenaCardSelectionPanel *state,
                     WenaBoardLayout *layout, struct nk_vec2 point)
{
    int down;
    for (down = 1; down >= 0; --down) {
        nk_clear(ctx);
        nk_input_begin(ctx);
        nk_input_motion(ctx, (int)point.x, (int)point.y);
        nk_input_button(ctx, NK_BUTTON_LEFT, (int)point.x, (int)point.y, down);
        nk_input_end(ctx);
        render(ctx, state, layout);
    }
}

static void click(struct nk_context *ctx, WenaCardSelectionPanel *state,
                  WenaBoardLayout *layout, const char *label)
{
    click_at(ctx, state, layout, label_center(ctx, label));
}

static unsigned int control_frame(struct nk_context *ctx,WenaCardSelection *selection,const WenaCard *card)
{
 unsigned int action;action=0;
 if(nk_begin(ctx,"Minicard",nk_rect(0,0,320,240),NK_WINDOW_BORDER))
  action=wena_card_selection_control(ctx,selection,card);
 nk_end(ctx);return action;
}
static void minicard_control(WenaCardSelection *selection,WenaCard *cards)
{
 struct nk_context ctx;struct nk_user_font font;struct nk_vec2 point;unsigned int action;int down,round;
 memset(&font,0,sizeof(font));font.height=14;font.width=text_width;assert(nk_init_default(&ctx,&font));
 for(round=0;round<2;++round){
  nk_clear(&ctx);nk_input_begin(&ctx);nk_input_end(&ctx);assert(!control_frame(&ctx,selection,&cards[0]));
  point=label_center(&ctx,"Selected:");action=0;
  for(down=1;down>=0;--down){
   nk_clear(&ctx);nk_input_begin(&ctx);nk_input_motion(&ctx,(int)point.x,(int)point.y);
   nk_input_button(&ctx,NK_BUTTON_LEFT,(int)point.x,(int)point.y,down);nk_input_end(&ctx);
   action|=control_frame(&ctx,selection,&cards[0]);
  }
  assert(action==WENA_CARD_BODY_TOGGLE_SELECTION&&selection->count==(size_t)round);
  assert(wena_card_selection_toggle(selection,cards,9,cards[0].id));
 }
 assert(!selection->count);
 assert(!wena_card_selection_selected(selection,&cards[0]));
 assert(wena_card_selection_toggle(selection,cards,9,cards[0].id));
 assert(wena_card_selection_selected(selection,&cards[0]));
 assert(!wena_card_selection_selected(selection,&cards[6])&&!wena_card_selection_selected(selection,&cards[7]));
 wena_card_selection_clear(selection);
 nk_clear(&ctx);nk_input_begin(&ctx);nk_input_end(&ctx);
 assert(!control_frame(&ctx,selection,&cards[6]));
 nk_clear(&ctx);nk_input_begin(&ctx);nk_input_end(&ctx);
 assert(!control_frame(&ctx,selection,&cards[7]));
 nk_free(&ctx);
}
/* The supplied visible order deliberately differs from cache order and skips
 * hidden cards. Real checkbox input emits intents without mutating the model. */
static unsigned int traversal_frame(struct nk_context *ctx,WenaCardSelectionTraversal *state,
 WenaCardSelection *selection,WenaCard *cards)
{
 unsigned int action;action=0;wena_card_selection_traversal_begin(state,selection);
 if(nk_begin(ctx,"Range",nk_rect(0,0,320,240),NK_WINDOW_BORDER)){
  action|=wena_card_selection_traversal_control(ctx,state,&cards[4]);
  action|=wena_card_selection_traversal_control(ctx,state,&cards[2]);
  action|=wena_card_selection_traversal_control(ctx,state,&cards[0]);
 }
 nk_end(ctx);return action;
}
static void traversal(WenaCardSelection *selection,WenaCard *cards)
{
 struct nk_context ctx;struct nk_user_font font;struct nk_vec2 point;
 WenaCardSelectionTraversal *state;WenaCardSelection *before;
 unsigned int action;int down;
 state=(WenaCardSelectionTraversal*)calloc(1,sizeof(*state));
 before=(WenaCardSelection*)malloc(sizeof(*before));assert(state&&before);
 memset(&font,0,sizeof(font));font.height=14;font.width=text_width;assert(nk_init_default(&ctx,&font));
 nk_input_begin(&ctx);nk_input_end(&ctx);assert(!traversal_frame(&ctx,state,selection,cards));
 point=label_center(&ctx,"Selected:");action=0;
 for(down=1;down>=0;--down){
  nk_clear(&ctx);nk_input_begin(&ctx);nk_input_motion(&ctx,(int)point.x,(int)point.y);
  nk_input_key(&ctx,NK_KEY_SHIFT,1);
  nk_input_button(&ctx,NK_BUTTON_LEFT,(int)point.x,(int)point.y,down);nk_input_end(&ctx);
  action|=traversal_frame(&ctx,state,selection,cards);
 }
 assert(action==WENA_CARD_BODY_RANGE_SELECTION&&!selection->count);
 /* With no anchor Shift-click toggles one card. */
 assert(wena_card_selection_traversal_apply(state,cards,9,"c4",action));
 assert(selection->count==1&&!strcmp(state->anchor,"c4"));
 assert(wena_card_selection_traversal_apply(state,cards,9,"c0",action));
 assert(selection->count==3&&!strcmp(selection->ids[0],"c4")&&!strcmp(selection->ids[1],"c2")&&!strcmp(selection->ids[2],"c0"));
 assert(!wena_card_selection_contains(selection,"c1")&&!wena_card_selection_contains(selection,"c3"));
 /* Repeated ranges retain anchor; reversing display order uses current order. */
 assert(wena_card_selection_traversal_apply(state,cards,9,"c2",action)&&selection->count==2);
 strcpy(state->ids[0],"c0");strcpy(state->ids[2],"c4");
 assert(wena_card_selection_traversal_apply(state,cards,9,"c0",action));
 assert(selection->count==3&&!strcmp(selection->ids[0],"c0")&&!strcmp(state->anchor,"c4"));
 memcpy(before,selection,sizeof(*before));
 assert(!wena_card_selection_traversal_apply(state,cards,9,"c1",action)&&!memcmp(before,selection,sizeof(*before)));
 /* Hidden anchor cannot pull hidden cards into a range. */
 state->count=1;strcpy(state->ids[0],"c1");
 assert(wena_card_selection_traversal_apply(state,cards,9,"c1",action));
 assert(selection->count==4&&!strcmp(state->anchor,"c1"));
 wena_card_selection_clear(selection);
 nk_clear(&ctx);nk_input_begin(&ctx);nk_input_key(&ctx,NK_KEY_SHIFT,0);nk_input_end(&ctx);
 assert(!traversal_frame(&ctx,state,selection,cards)&&!state->anchor[0]);
 memcpy(before,selection,sizeof(*before));
 /* Duplicate callback visits invalidate the entire frame, including prior intents. */
 if(nk_begin(&ctx,"Duplicate",nk_rect(330,0,300,240),NK_WINDOW_BORDER))
  assert(!wena_card_selection_traversal_control(&ctx,state,&cards[4]));
 nk_end(&ctx);assert(state->error);
 assert(!wena_card_selection_traversal_apply(state,cards,9,"c4",action)&&!memcmp(before,selection,sizeof(*before)));
 nk_free(&ctx);free(before);free(state);
}
int main(void)
{
 struct nk_context ctx;struct nk_user_font font;WenaBoard board;WenaCard cards[9];
 WenaBoardLayout layout;WenaCardSelectionPanel state;WenaCardSelection *selection,*before;
 size_t i;char id[8];
 selection=(WenaCardSelection*)malloc(sizeof(*selection));before=(WenaCardSelection*)malloc(sizeof(*before));assert(selection&&before);
 assert(wena_card_selection_init(selection,"board"));wena_card_selection_panel_init(&state,selection);
 assert(wena_board_init(&board,"board","Board",0));memset(&layout,0,sizeof(layout));
 for(i=0;i<6;++i){sprintf(id,"c%lu",(unsigned long)i);assert(wena_card_init(&cards[i],id,"board","lane","list","Repeated",(double)i,0));}
 assert(wena_card_init(&cards[6],"foreign","other","lane","list","Foreign",6,0));
 assert(wena_card_init(&cards[7],"archived","board","lane","list","Archived",7,1));
 assert(wena_card_init(&cards[8],"outside","board","other","list","Outside",8,0));
 minicard_control(selection,cards);
 traversal(selection,cards);
 layout.board=&board;layout.cards=cards;layout.card_count=9;
 memset(&font,0,sizeof(font));font.height=14;font.width=text_width;assert(nk_init_default(&ctx,&font));
 assert(wena_card_selection_panel_open(&state,cards,9,"board","list","lane")&&selection->count==6);
 nk_input_begin(&ctx);nk_input_end(&ctx);render(&ctx,&state,&layout);
 click(&ctx,&state,&layout,"Next Page");assert(state.table.page==1&&selection->count==6);
 click(&ctx,&state,&layout,"Repeated [c5]");assert(selection->count==5&&!wena_card_selection_contains(selection,"c5"));
 click(&ctx,&state,&layout,"Previous Page");assert(!state.table.page&&selection->count==5);
 click(&ctx,&state,&layout,"Select none");assert(!selection->count&&state.visible);
 click(&ctx,&state,&layout,"Select all");assert(selection->count==6&&!wena_card_selection_contains(selection,"outside"));
 click(&ctx,&state,&layout,"Next Page");assert(state.table.page==1);
 for(i=1;i<6;++i)cards[i].archived=1;
 nk_clear(&ctx);nk_input_begin(&ctx);nk_input_end(&ctx);render(&ctx,&state,&layout);
 assert(!state.table.page&&selection->count==1);
 memcpy(before,selection,sizeof(*before));cards[0].archived=2;
 nk_clear(&ctx);nk_input_begin(&ctx);nk_input_end(&ctx);render(&ctx,&state,&layout);
 assert(state.error&&!memcmp(before,selection,sizeof(*before)));
 cards[0].archived=0;
 nk_clear(&ctx);nk_input_begin(&ctx);nk_input_end(&ctx);render(&ctx,&state,&layout);
 assert(!state.error);
 assert(!wena_card_selection_panel_open(&state,cards,9,"other","list","lane")&&state.visible&&!memcmp(before,selection,sizeof(*before)));
 cards[0].archived=1;
 nk_clear(&ctx);nk_input_begin(&ctx);nk_input_end(&ctx);render(&ctx,&state,&layout);
 assert(!selection->count);(void)label_center(&ctx,"No items.");
 click(&ctx,&state,&layout,"Turn Multi-Selection off");assert(!state.visible&&!selection->count);
 cards[0].archived=0;assert(wena_card_selection_panel_open(&state,cards,9,"board","list",NULL));
 nk_clear(&ctx);nk_input_begin(&ctx);nk_input_end(&ctx);render(&ctx,&state,&layout);
 nk_clear(&ctx);nk_input_begin(&ctx);nk_input_key(&ctx,NK_KEY_TEXT_RESET_MODE,1);nk_input_end(&ctx);render(&ctx,&state,&layout);
 assert(!state.visible&&!selection->count);
 assert(wena_card_selection_panel_open(&state,cards,9,"board","list",NULL));
 assert(!wena_card_selection_panel_render(&ctx,&state,cards,9,"other",640,480)&&!state.visible&&!selection->count);
 nk_free(&ctx);free(before);free(selection);puts("Real paginated card selection, scoped toggles, pruning, errors and Escape passed");return 0;
}
