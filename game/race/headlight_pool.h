/* port (psx_headlights, the night's second look): a pool of light ahead of each car with headlights. Under the night the world's nodes are dark (HEADLIGHT_FLOOR
 * of their light, 256 = full; a tunnel's less dark, TUNNEL_FLOOR, its own lamps stay on) and the road and the walls ahead of a lit car are lifted back to their own
 * light: nothing at the car itself, rising over HEADLIGHT_START..HEADLIGHT_START + HEADLIGHT_RISE nodes in front of it, full to HEADLIGHT_FULL and falling smoothly to
 * the dark at HEADLIGHT_AHEAD. The car's place is fractional (node + frac / 256), so the pool slides with the car instead of stepping a section at a time. The light
 * bytes psx_light_at gives are scaled by the pool's gain at the node, so the road is lit only ahead of the car, the scenery there takes it (psx_light_node) and the
 * cars ahead take it too (headlight_pool_gain at their node, as a multiplier of their ambient). Off, nothing is scaled: option 1's look. */
#ifndef TNFS_HEADLIGHT_POOL_H
#define TNFS_HEADLIGHT_POOL_H

#include <stdint.h>

enum {
    HEADLIGHT_MAX = 8,       /* cars that light the road */
    HEADLIGHT_AHEAD = 14,    /* nodes the light reaches ahead of a car */
    HEADLIGHT_START = 1,     /* nodes ahead of the car's place where the light begins (the car's own section and what is behind it stay dark) */
    HEADLIGHT_RISE = 2,      /* nodes over which it rises to full */
    HEADLIGHT_CAR_SHARE = 176, /* of 256: the share of the pool's lift a car's own colours take */
    HEADLIGHT_FULL = 5,      /* nodes ahead of the car's place to which it stays full */
    HEADLIGHT_FLOOR = 96,    /* the dark's share of the light, out of 256: the night's luminance over the dusk's palette (drive_light.c NIGHT_PALETTE) */
    TUNNEL_BEAM = 150,       /* the light, out of 255, a tunnel node takes at the beam's full when its own rows are darker (a tunnel with no lamps is black at night) */
    TUNNEL_FLOOR = 150       /* a tunnel's: its own lamps keep it lit, the beam still shows on it */
};

typedef struct {
    int32_t node[HEADLIGHT_MAX];   /* the nodes of the cars with their headlights on */
    int16_t sub[HEADLIGHT_MAX];    /* how far each car is on past its node, in 256ths of a node (0 when the caller keeps whole nodes): the pool slides, it does not step */
    int8_t dir[HEADLIGHT_MAX];     /* +1 the car travels to higher nodes, -1 to lower (reversing, or the wrong way round); 0 counts as +1: the light is ahead of the car's travel */
    int count;
    uint32_t ring;                 /* the track's nodes: a node ahead wraps round it */
    const uint8_t *tunnel;         /* per node (ring of them), 1 where the road itself is a tunnel (its property bit, psx_light_synth_tunnels); NULL when not known */
} HeadlightPool;

/* How much of its light a node has (a tunnel node of pool->tunnel with TUNNEL_FLOOR): 256 for full, HEADLIGHT_FLOOR for none of the pool's; the best of the cars' pools. A pool with no cars is all dark. */
int headlight_pool_gain(const HeadlightPool *pool, uint32_t node);

/* 1 when pool->tunnel marks `node` a tunnel */
int headlight_pool_tunnel(const HeadlightPool *pool, uint32_t node);

/* The share (0..256) of a pool's lift that a point of a node's row takes: the road and its verge whole, the ground beyond falling to nothing, so the pool is as wide as the
 * road and its shoulders (point numbering as psx_light.h: 0, 1, 6 the road, 2 and 7 the shoulders' far edge, 3..5 and 8..10 beyond). */
int headlight_pool_width(int point);

/* The gain a car's own colours take from the pool gain `gain` at its node (a car is lit less than the road under its own lamps: HEADLIGHT_CAR_SHARE of the lift). */
int headlight_pool_car_gain(int gain);

/* the same as a tunnel node, whatever pool->tunnel says (a light whose own rows mark the node a tunnel) */
int headlight_pool_gain_tunnel(const HeadlightPool *pool, uint32_t node);

/* 1 when a car with its headlights on is at `node`: the car's own place takes the light the beam leaves out ahead of it (its body and the dash stay lit) */
int headlight_pool_car_at(const HeadlightPool *pool, uint32_t node);

/* how much of the beam a node gets, 0..256 (0 outside every car's beam), whatever the floor */
int headlight_pool_beam(const HeadlightPool *pool, uint32_t node);

/* `value` (0..255) scaled by `gain` (256 = whole). */
uint8_t headlight_pool_apply(uint8_t value, int gain);

#endif
