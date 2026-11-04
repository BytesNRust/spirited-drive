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

/* one car's patch of light at a ground point: a fan ahead along the nose, brightest close to the car, full to CONE_HOLD_M and falling smoothly to nothing at CONE_REACH_M, with a half-width
 * that grows with the distance and fades across (a smoothstep from the axis, no plateau, no edge); and a faint glow round the car and just behind it (a disc of radius CONE_GLOW_M
 * at CONE_GLOW_SHARE of the beam). The best of the two. */
int headlight_cone_weight_local(double forward_m, double lateral_m, double height_m)
{
    const double along = forward_m, across = fabs(lateral_m);
    int best = 0;
    const double r = sqrt(forward_m * forward_m + lateral_m * lateral_m);
    if (r < CONE_GLOW_M) best = CONE_GLOW_SHARE * (256 - smooth((int)(256.0 * r / CONE_GLOW_M))) / 256;
    if (along > -CONE_BACK_M && along < CONE_REACH_M) {
        int d = 256;
        if (along < 0.0) d = smooth((int)(256.0 * (along + CONE_BACK_M) / CONE_BACK_M));   /* the fan is born just behind the nose: no edge across the car */
        if (along > CONE_HOLD_M) d = smooth(256 - (int)(256.0 * (along - CONE_HOLD_M) / (CONE_REACH_M - CONE_HOLD_M)));
        const double half = CONE_WIDTH_BASE + (along > 0.0 ? along : 0.0) * CONE_WIDTH_PERCENT / 100.0;
        if (across < half) {
            const int l = d * (256 - smooth((int)(256.0 * across / half))) / 256;
            if (l > best) best = l;
        }
    }
    const double h = height_m < 0.0 ? 0.0 : height_m;   /* the light thins with the height above the ground: full to CONE_HEIGHT_FULL_M, none from CONE_HEIGHT_END_M */
    if (h >= CONE_HEIGHT_END_M) return 0;
    if (h > CONE_HEIGHT_FULL_M) best = best * (256 - smooth((int)(256.0 * (h - CONE_HEIGHT_FULL_M) / (CONE_HEIGHT_END_M - CONE_HEIGHT_FULL_M)))) / 256;
    return best;
}

int headlight_car_lights(int drawn_lately, int32_t behind, int cone)
{
    return drawn_lately || (cone && behind >= 0 && behind <= HEADLIGHT_UNDRAWN_NODES);
}

/* one car's patch of light at a ground point, in the car's own frame */
static int cone_lift(const HeadlightPool *pool, int car, double x, double z)
{
    const double dx = x - pool->cone_x[car], dz = z - pool->cone_z[car];
    return headlight_cone_weight_local(dx * pool->cone_hx[car] + dz * pool->cone_hz[car], dx * pool->cone_hz[car] - dz * pool->cone_hx[car], 0.0);
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
