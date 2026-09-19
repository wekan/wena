#ifndef WENA_LABELS_COMPONENT_H
#define WENA_LABELS_COMPONENT_H

struct nk_context;
/* A label button uses the canonical background and black/white contrast text.
 * The caller owns layout and decides whether the returned click is actionable.
 * Invalid colors fail closed; empty stored colors use the canonical fallback. */
int wena_label_badge_render(struct nk_context *context, const char *name,
    const char *color);

#endif
