#include "../models/wip_limit.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void decisions(void)
{
    WenaWipLimit limit;
    WenaWipDecision result, before;
    size_t value, current, removed, added, projected;
    int enabled, soft, allowed;
    for (enabled = 0; enabled <= 1; ++enabled)
        for (soft = 0; soft <= 1; ++soft)
            for (value = 1; value <= 5; ++value)
                for (current = 0; current <= 8; ++current)
                    for (removed = 0; removed <= current; ++removed)
                        for (added = 0; added <= 8; ++added) {
                            limit.value = value;
                            limit.enabled = enabled;
                            limit.soft = soft;
                            projected = current - removed + added;
                            allowed = 1;
                            if (enabled && !soft && added > removed &&
                                projected > value) allowed = 0;
                            assert(wena_wip_evaluate(&limit, current, removed,
                                added, &result));
                            assert(result.projected == projected);
                            assert(result.allowed == allowed);
                            assert(result.reached == (enabled && projected >= value));
                            assert(result.exceeded == (enabled && projected > value));
                        }
    wena_wip_limit_init(&limit);
    limit.enabled = 1;
    limit.value = (size_t)-1;
    assert(wena_wip_evaluate(&limit, (size_t)-1, 1, 1, &result));
    assert(result.projected == (size_t)-1 && result.reached &&
        !result.exceeded && result.allowed);
    memset(&result, 42, sizeof(result));
    memcpy(&before, &result, sizeof(result));
    assert(!wena_wip_evaluate(&limit, (size_t)-1, 0, 1, &result));
    assert(!wena_wip_evaluate(&limit, 0, 1, 0, &result));
    assert(!wena_wip_evaluate(NULL, 0, 0, 0, &result));
    assert(!wena_wip_evaluate(&limit, 0, 0, 0, NULL));
    limit.value = 0;
    assert(!wena_wip_evaluate(&limit, 0, 0, 0, &result));
    limit.value = 1;
    limit.enabled = 2;
    assert(!wena_wip_evaluate(&limit, 0, 0, 0, &result));
    limit.enabled = 0;
    limit.soft = -1;
    assert(!wena_wip_evaluate(&limit, 0, 0, 0, &result));
    assert(memcmp(&before, &result, sizeof(result)) == 0);
}

static void editor(void)
{
    WenaWipLimit limit, result, before;
    size_t value, count;
    int enabled, soft, valid;
    wena_wip_limit_init(NULL);
    wena_wip_limit_init(&limit);
    assert(limit.value == 1 && !limit.enabled && !limit.soft);
    assert(wena_wip_limit_valid(&limit));
    for (enabled = 0; enabled <= 1; ++enabled)
        for (soft = 0; soft <= 1; ++soft)
            for (count = 0; count <= 101; ++count) {
                limit.enabled = enabled;
                limit.soft = soft;
                limit.value = 3;
                for (value = 0; value <= 100; ++value) {
                    memset(&result, 42, sizeof(result));
                    memcpy(&before, &result, sizeof(result));
                    valid = value >= 1 && value <= 99 && (soft || value >= count);
                    assert(wena_wip_edit(&limit, WENA_WIP_APPLY_VALUE,
                        value, count, &result) == valid);
                    if (valid) {
                        assert(result.value == value && result.enabled == enabled &&
                            result.soft == soft);
                    } else assert(memcmp(&before, &result, sizeof(result)) == 0);
                }
                result = limit;
                assert(wena_wip_edit(&result, WENA_WIP_TOGGLE_ENABLED,
                    0, count, &result));
                assert(result.enabled == !enabled && result.soft == soft);
                assert(result.value == (!enabled && count > 3 ? count : 3));
                result = limit;
                assert(wena_wip_edit(&result, WENA_WIP_TOGGLE_SOFT,
                    0, count, &result));
                assert(result.enabled == enabled && result.soft == !soft);
                assert(result.value == (soft && count > 3 ? count : 3));
            }
    wena_wip_limit_init(&limit);
    assert(wena_wip_edit(&limit, WENA_WIP_TOGGLE_ENABLED, 0, (size_t)-1, &limit));
    assert(limit.value == (size_t)-1 && limit.enabled);
    memset(&result, 42, sizeof(result));
    memcpy(&before, &result, sizeof(result));
    assert(!wena_wip_edit(NULL, WENA_WIP_APPLY_VALUE, 1, 0, &result));
    assert(!wena_wip_edit(&limit, WENA_WIP_APPLY_VALUE, 1, 0, NULL));
    assert(!wena_wip_edit(&limit, (WenaWipEdit)99, 1, 0, &result));
    assert(!wena_wip_edit(&limit, WENA_WIP_APPLY_VALUE, (size_t)-1, 0, &result));
    limit.enabled = -1;
    assert(!wena_wip_edit(&limit, WENA_WIP_TOGGLE_ENABLED, 0, 0, &result));
    assert(memcmp(&before, &result, sizeof(result)) == 0);
}

int main(void)
{
    decisions();
    editor();
    puts("wip-limit: shared hard/soft decisions, editor transitions and bounds passed");
    return 0;
}
