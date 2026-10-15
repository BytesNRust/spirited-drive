#include "game/race/race_teardown.h"

void race_scenery_state_free(const RaceFreeOps *ops, uint32_t *scenery_state)
{
    ops->free(ops->ctx, *scenery_state);
    *scenery_state = 0;
}

void race_rrsm_free(const RaceFreeOps *ops, uint32_t *rrsm)
{
    if (*rrsm == 0) return;
    ops->free(ops->ctx, *rrsm);
    *rrsm = 0;
}
