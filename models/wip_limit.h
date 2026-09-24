#ifndef WENA_WIP_LIMIT_H
#define WENA_WIP_LIMIT_H

#include <stddef.h>

typedef struct WenaWipLimit {
    size_t value;
    int enabled;
    int soft;
} WenaWipLimit;

typedef struct WenaWipDecision {
    size_t projected;
    /* Status after the proposed change; disabled limits never warn. */
    int reached;
    int exceeded;
    int allowed;
} WenaWipDecision;

typedef enum WenaWipEdit {
    WENA_WIP_APPLY_VALUE,
    WENA_WIP_TOGGLE_ENABLED,
    WENA_WIP_TOGGLE_SOFT
} WenaWipEdit;

void wena_wip_limit_init(WenaWipLimit *limit);
int wena_wip_limit_valid(const WenaWipLimit *limit);
/* Pure shared arithmetic for a list, lane or group of lists. Count active,
 * unfiltered cards in the entire scope. A move within that scope removes and
 * adds one; creation/restoration only adds one. Non-increasing changes remain
 * allowed even when already over a hard limit. Reject underflow/overflow and
 * malformed settings, preserving output. No authorization or storage access. */
int wena_wip_evaluate(const WenaWipLimit *limit, size_t current,
    size_t removed, size_t added, WenaWipDecision *output);
/* WeKan list editor rules: explicit values are 1..99 and hard values cannot
 * be below current count, even while disabled. Enabling or switching soft to
 * hard raises a smaller saved value to current count (possibly above 99).
 * value is used only by APPLY_VALUE. Supports input == output; failure leaves
 * output unchanged. Persist only after the caller checks scope and revision. */
int wena_wip_edit(const WenaWipLimit *limit, WenaWipEdit edit,
    size_t value, size_t current, WenaWipLimit *output);

#endif
