#ifndef WENA_POSITION_INPUT_H
#define WENA_POSITION_INPUT_H
struct nk_context;
/* Display 1..count, retain zero-based domain ordinals. Disabled keeps geometry. */
int wena_position_input(struct nk_context*,int selected,int count,int enabled);
#endif
