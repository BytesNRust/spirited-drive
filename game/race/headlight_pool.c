#include "game/race/headlight_pool.h"

#include <math.h>
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

/* one car's cone at a ground point: the lift of its distance ahead along the nose, times the falloff across it */
static int cone_lift(const HeadlightPool *pool, int car, double x, double z)
{
    const double dx = x - pool->cone_x[car], dz = z - pool->cone_z[car];
    const double along = dx * pool->cone_hx[car] + dz * pool->cone_hz[car];
    const double across = fabs(dx * pool->cone_hz[car] - dz * pool->cone_hx[car]);
    const int d = lift((int)(along * 256.0 / CONE_METRES_PER_NODE));
    if (d == 0) return 0;
    const double half = CONE_WIDTH_BASE + along * CONE_WIDTH_PERCENT / 100.0;
    if (across >= half) return 0;
    const double edge = 0.55 * half;
    if (across <= edge) return d;
    return d * (256 - smooth((int)(256.0 * (across - edge) / (half - edge)))) / 256;
}

int headlight_pool_lift_xz(const HeadlightPool *pool, double x, double z)
{
    int best = 0;
    if (pool == NULL) return 0;
    for (int i = 0; i < pool->count && i < HEADLIGHT_MAX; i++) {
        const int l = cone_lift(pool, i, x, z);
        if (l > best) best = l;
    }
    return best;
}

int headlight_pool_gain_at_lift(int floor, int lift) { return floor + (256 - floor) * lift / 256; }

static int best_lift(const HeadlightPool *pool, uint32_t node)
{
    int best = 0;
    if (pool == NULL) return 0;
    if (pool->cone && pool->node_xz != NULL && pool->ring != 0)   /* port (headlight_cone): the node's centre in the cars' cones */
        return headlight_pool_lift_xz(pool, pool->node_xz[2 * (node % pool->ring)] / 65536.0, pool->node_xz[2 * (node % pool->ring) + 1] / 65536.0);
    for (int i = 0; i < pool->count && i < HEADLIGHT_MAX; i++) {
        int32_t d = (int32_t)(node % (pool->ring != 0 ? pool->ring : 1)) - pool->node[i];
        if (pool->ring != 0) {   /* the nearest way round the ring */
            const int32_t half = (int32_t)(pool->ring / 2);
            if (d > half) d -= (int32_t)pool->ring;
            else if (d < -half) d += (int32_t)pool->ring;
        }
        int32_t d256 = d * 256 - pool->sub[i];
        if (pool->dir[i] < 0) d256 = -d256;   /* the light is ahead of the car's travel */
        const int l = lift(d256);
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

int headlight_pool_width(int point)
{
    static const int SHARE[11] = { 256, 256, 256, 112, 48, 0, 256, 256, 112, 48, 0 };
    return point >= 0 && point < 11 ? SHARE[point] : 256;
}

int headlight_pool_car_gain(int gain)
{
    return HEADLIGHT_FLOOR + (gain - HEADLIGHT_FLOOR) * HEADLIGHT_CAR_SHARE / 256;
}
