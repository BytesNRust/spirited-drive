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
    HEADLIGHT_FULL = 5,      /* nodes ahead of the car's place to which it stays full */
    HEADLIGHT_FLOOR = 96,    /* the dark's share of the light, out of 256: the night's luminance over the dusk's palette (drive_light.c NIGHT_PALETTE) */
    TUNNEL_FLOOR = 150       /* a tunnel's: its own lamps keep it lit, the beam still shows on it */
};

typedef struct {
    int32_t node[HEADLIGHT_MAX];   /* the nodes of the cars with their headlights on */
    uint8_t frac[HEADLIGHT_MAX];   /* how far each is on to the next node, out of 256 (0 when not known) */
    int count;
    uint32_t ring;                 /* the track's nodes: a node ahead wraps round it */
} HeadlightPool;

/* How much of its light a node has: 256 for full, HEADLIGHT_FLOOR for none of the pool's; the best of the cars' pools. A pool with no cars is all dark. */
int headlight_pool_gain(const HeadlightPool *pool, uint32_t node);

/* the same inside a tunnel (TUNNEL_FLOOR) */
int headlight_pool_gain_tunnel(const HeadlightPool *pool, uint32_t node);

/* `value` (0..255) scaled by `gain` (256 = whole). */
uint8_t headlight_pool_apply(uint8_t value, int gain);

#endif
