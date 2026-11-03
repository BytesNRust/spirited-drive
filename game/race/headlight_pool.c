#include "game/race/headlight_pool.h"

#include <stddef.h>

/* the lift 0..256 at `ahead` 1/256 nodes in front of a car's place (negative: behind it): none behind it and under the first HEADLIGHT_START nodes, a smoothstep up
 * to 256 over HEADLIGHT_RISE, full to HEADLIGHT_FULL, a smoothstep down to 0 at HEADLIGHT_AHEAD */
static int smooth(int t) { return t * t * (3 * 256 - 2 * t) / (256 * 256); }

static int lift(int ahead)
{
    const int from = HEADLIGHT_START * 256, full = (HEADLIGHT_START + HEADLIGHT_RISE) * 256, hold = HEADLIGHT_FULL * 256, end = HEADLIGHT_AHEAD * 256;
    if (ahead <= from || ahead >= end) return 0;
    if (ahead < full) return smooth(256 * (ahead - from) / (full - from));
    if (ahead <= hold) return 256;
    return smooth(256 - 256 * (ahead - hold) / (end - hold));
}

static int best_lift(const HeadlightPool *pool, uint32_t node)
{
    int best = 0;
    if (pool == NULL) return 0;
    for (int i = 0; i < pool->count && i < HEADLIGHT_MAX; i++) {
        int32_t d = (int32_t)(node % (pool->ring != 0 ? pool->ring : 1)) - pool->node[i];
        if (pool->ring != 0) {   /* the nearest way round the ring */
            const int32_t half = (int32_t)(pool->ring / 2);
            if (d > half) d -= (int32_t)pool->ring;
            else if (d < -half) d += (int32_t)pool->ring;
        }
        const int l = lift(d * 256 - pool->frac[i]);
        if (l > best) best = l;
    }
    return best;
}

static int gain_with(const HeadlightPool *pool, uint32_t node, int floor)
{
    if (pool == NULL) return 256;
    return floor + (256 - floor) * best_lift(pool, node) / 256;
}

int headlight_pool_beam(const HeadlightPool *pool, uint32_t node) { return best_lift(pool, node); }

int headlight_pool_gain(const HeadlightPool *pool, uint32_t node)
{
    return gain_with(pool, node, headlight_pool_tunnel(pool, node) ? TUNNEL_FLOOR : HEADLIGHT_FLOOR);
}

int headlight_pool_tunnel(const HeadlightPool *pool, uint32_t node)
{
    return pool != NULL && pool->tunnel != NULL && pool->ring != 0 && pool->tunnel[node % pool->ring];
}

int headlight_pool_car_at(const HeadlightPool *pool, uint32_t node)
{
    if (pool == NULL) return 0;
    for (int i = 0; i < pool->count && i < HEADLIGHT_MAX; i++)
        if ((uint32_t)pool->node[i] == (pool->ring != 0 ? node % pool->ring : node)) return 1;
    return 0;
}

int headlight_pool_gain_tunnel(const HeadlightPool *pool, uint32_t node) { return gain_with(pool, node, TUNNEL_FLOOR); }

uint8_t headlight_pool_apply(uint8_t value, int gain)
{
    const int v = value * gain / 256;
    return (uint8_t)(v > 255 ? 255 : v);
}
