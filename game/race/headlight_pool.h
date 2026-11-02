/* port (psx_headlights, the night's second look): a pool of light ahead of each car with headlights. Under the night the world's nodes are dark (HEADLIGHT_FLOOR
 * of their light, 256 = full) and the road and the walls round a lit car are lifted back to their own light over HEADLIGHT_AHEAD nodes ahead of it (full for the
 * first HEADLIGHT_FULL, falling smoothly to the dark), a little behind it too (HEADLIGHT_BEHIND), the way psx_light_synth steps a tunnel's rows. The light bytes
 * psx_light_at gives are scaled by the pool's gain at the node, so the road is lit only near the car, the scenery there takes it (psx_light_node) and the cars
 * ahead take it too (headlight_pool_gain at their node, as a multiplier of their ambient). Off, nothing is scaled: option 1's look. */
#ifndef TNFS_HEADLIGHT_POOL_H
#define TNFS_HEADLIGHT_POOL_H

#include <stdint.h>

enum {
    HEADLIGHT_MAX = 8,       /* cars that light the road */
    HEADLIGHT_AHEAD = 14,    /* nodes the light reaches ahead of a car */
    HEADLIGHT_FULL = 3,      /* of those, nodes at full light from the car's */
    HEADLIGHT_BEHIND = 2,    /* nodes behind a car that it still lights, falling to the dark */
    HEADLIGHT_CAR_SHARE = 176, /* of 256: the share of the pool's lift a car's own colours take */
    HEADLIGHT_FLOOR = 96     /* the dark's share of the light, out of 256: the night's luminance over the dusk's palette (drive_light.c NIGHT_PALETTE) */
};

typedef struct {
    int32_t node[HEADLIGHT_MAX];   /* the nodes of the cars with their headlights on */
    int16_t sub[HEADLIGHT_MAX];    /* the car's place past its node in 256ths of a node (0 when the caller keeps whole nodes): the pool glides, it does not step */
    int8_t dir[HEADLIGHT_MAX];     /* +1 the car travels to higher nodes, -1 to lower (reversing, or the wrong way round); 0 counts as +1: the light is ahead of the car's travel */
    int count;
    uint32_t ring;                 /* the track's nodes: a node ahead wraps round it */
} HeadlightPool;

/* How much of its light a node has: 256 for full, HEADLIGHT_FLOOR for none of the pool's; the best of the cars' pools. A pool with no cars is all dark. */
int headlight_pool_gain(const HeadlightPool *pool, uint32_t node);

/* The share (0..256) of a pool's lift that a point of a node's row takes: the road and its verge whole, the ground beyond falling to nothing, so the pool is as wide as the
 * road and its shoulders (point numbering as psx_light.h: 0, 1, 6 the road, 2 and 7 the shoulders' far edge, 3..5 and 8..10 beyond). */
int headlight_pool_width(int point);

/* The gain a car's own colours take from the pool gain `gain` at its node (a car is lit less than the road under its own lamps: HEADLIGHT_CAR_SHARE of the lift). */
int headlight_pool_car_gain(int gain);

/* `value` (0..255) scaled by `gain` (256 = whole). */
uint8_t headlight_pool_apply(uint8_t value, int gain);

#endif
