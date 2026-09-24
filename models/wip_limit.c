#include "wip_limit.h"

void wena_wip_limit_init(WenaWipLimit *limit)
{
    if (!limit) return;
    limit->value = 1;
    limit->enabled = 0;
    limit->soft = 0;
}

int wena_wip_limit_valid(const WenaWipLimit *limit)
{
    return limit && limit->value > 0 &&
        (limit->enabled == 0 || limit->enabled == 1) &&
        (limit->soft == 0 || limit->soft == 1);
}

int wena_wip_evaluate(const WenaWipLimit *limit, size_t current,
    size_t removed, size_t added, WenaWipDecision *output)
{
    WenaWipDecision candidate;
    size_t remaining;
    if (!output || !wena_wip_limit_valid(limit) || removed > current)
        return 0;
    remaining = current - removed;
    if (added > (size_t)-1 - remaining) return 0;
    candidate.projected = remaining + added;
    candidate.reached = limit->enabled && candidate.projected >= limit->value;
    candidate.exceeded = limit->enabled && candidate.projected > limit->value;
    candidate.allowed = !candidate.exceeded || limit->soft ||
        candidate.projected <= current;
    *output = candidate;
    return 1;
}

int wena_wip_edit(const WenaWipLimit *limit, WenaWipEdit edit,
    size_t value, size_t current, WenaWipLimit *output)
{
    WenaWipLimit candidate;
    if (!output || !wena_wip_limit_valid(limit)) return 0;
    candidate = *limit;
    switch (edit) {
    case WENA_WIP_APPLY_VALUE:
        if (value < 1 || value > 99 || (!limit->soft && value < current))
            return 0;
        candidate.value = value;
        break;
    case WENA_WIP_TOGGLE_ENABLED:
        candidate.enabled = !limit->enabled;
        if (candidate.enabled && candidate.value < current)
            candidate.value = current;
        break;
    case WENA_WIP_TOGGLE_SOFT:
        candidate.soft = !limit->soft;
        if (!candidate.soft && candidate.value < current)
            candidate.value = current;
        break;
    default:
        return 0;
    }
    *output = candidate;
    return 1;
}
