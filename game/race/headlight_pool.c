#include "game/race/headlight_pool.h"

#include <stddef.h>

/* the lift 0..256 at `distance` nodes from a car (negative: behind it): 256 under the car and HEADLIGHT_FULL ahead, a smoothstep down to 0 at HEADLIGHT_AHEAD
 * ahead (or HEADLIGHT_BEHIND behind) */
static int lift(int distance)
{
    if (distance < -HEADLIGHT_BEHIND || distance > HEADLIGHT_AHEAD) return 0;
    if (distance >= 0 && distance <= HEADLIGHT_FULL) return 256;
    const int span = distance > 0 ? HEADLIGHT_AHEAD - HEADLIGHT_FULL : HEADLIGHT_BEHIND + 1;
    const int into = distance > 0 ? distance - HEADLIGHT_FULL : -distance;   /* 1..span */
    const int t = 256 - 256 * into / span;                                  /* 256 near, 0 far */
    return t * t * (3 * 256 - 2 * t) / (256 * 256);                         /* smoothstep, still 0..256 */
}

/* the lift at a distance of `d256` 256ths of a node ahead (negative: behind): between the whole nodes' lifts, linearly */
static int lift_at(int32_t d256)
{
    const int32_t whole = d256 >= 0 ? d256 / 256 : -((-d256 + 255) / 256);   /* floor */
    const int32_t part = d256 - whole * 256;
    return (lift((int)whole) * (256 - part) + lift((int)whole + 1) * part) / 256;
}

int headlight_pool_gain(const HeadlightPool *pool, uint32_t node)
{
    int best = 0;
    if (pool == NULL) return 256;
    for (int i = 0; i < pool->count && i < HEADLIGHT_MAX; i++) {
        int32_t d = (int32_t)(node % (pool->ring != 0 ? pool->ring : 1)) - pool->node[i];
        if (pool->ring != 0) {   /* the nearest way round the ring */
            const int32_t half = (int32_t)(pool->ring / 2);
            if (d > half) d -= (int32_t)pool->ring;
            else if (d < -half) d += (int32_t)pool->ring;
        }
        int32_t d256 = d * 256 - pool->sub[i];
        if (pool->dir[i] < 0) d256 = -d256;   /* the light is ahead of the car's travel */
        const int l = lift_at(d256);
        if (l > best) best = l;
    }
    return HEADLIGHT_FLOOR + (256 - HEADLIGHT_FLOOR) * best / 256;
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

uint8_t headlight_pool_apply(uint8_t value, int gain)
{
    const int v = value * gain / 256;
    return (uint8_t)(v > 255 ? 255 : v);
}
