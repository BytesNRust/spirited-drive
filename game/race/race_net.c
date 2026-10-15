/* The race's side of the network game (race_net.h). */
#include "game/race/race_net.h"

int32_t race_net_wait_step(const RaceNetOps *ops, const RaceNetWords *w)
{
    ops->keys(ops->ctx);
    if (w->dropped != 0) return 1;
    if (w->deadline != RACE_NET_NO_DEADLINE && w->deadline < w->clock) return 1;
    ops->service(ops->ctx);
    return 0;
}

void race_net_count_down(RaceNetCounts *c, int32_t kind)
{
    if (kind != 0) c->unknown_4bc6d8 = (int16_t)(c->unknown_4bc6d8 - 1);
    else c->unknown_4bc6da = (int16_t)(c->unknown_4bc6da - 1);
}
