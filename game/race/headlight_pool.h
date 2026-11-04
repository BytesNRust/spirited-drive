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
    int cone;                      /* port (headlight_cone): 1 the beam is each car's cone (below) in metres, not the nodes' pool; 0 the nodes' pool as it was */
    double cone_x[HEADLIGHT_MAX], cone_z[HEADLIGHT_MAX];    /* each car's place on the ground, metres */
    double cone_hx[HEADLIGHT_MAX], cone_hz[HEADLIGHT_MAX];  /* and its heading, a unit vector (the nose, not the track's direction) */
    const int32_t *node_xz;        /* cone: the nodes' centres (x, z) in 16.16 metres, two per node: what a caller that knows only a node is lit at; NULL when not known */
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

/* port (headlight_cone): how much of the beam (0..256) a ground point (x, z) in metres gets from the cars' lights, as one patch that travels with each car: a fan ahead of its nose,
 * brightest close to it, full to CONE_HOLD_M and falling smoothly to nothing at CONE_REACH_M, as wide as CONE_WIDTH_BASE + CONE_WIDTH_PERCENT of the distance each side and fading
 * across; and a faint glow (CONE_GLOW_SHARE of the beam, radius CONE_GLOW_M) round the car and behind it. Smooth in metres everywhere, so no row of the road shows it; the best of the cars. */
enum { CONE_HOLD_M = 12, CONE_REACH_M = 100, CONE_WIDTH_BASE = 10, CONE_WIDTH_PERCENT = 60, CONE_BACK_M = 8, CONE_GLOW_M = 14, CONE_GLOW_SHARE = 80 };
int headlight_pool_lift_xz(const HeadlightPool *pool, double x, double z);

/* port (headlight_cone): one car's patch of light at a point in the car's own frame, metres: forward along its nose, lateral (either sign, symmetric), height above the ground. 0..256,
 * pure (no track, node or world lookup). Height thins it: full to CONE_HEIGHT_FULL_M, none from CONE_HEIGHT_END_M. headlight_pool_lift_xz is this per car at ground level. */
enum { CONE_HEIGHT_FULL_M = 3, CONE_HEIGHT_END_M = 12 };
int headlight_cone_weight_local(double forward_m, double lateral_m, double height_m);

/* the lamps' places in the car's own frame (the nose's two lamps, metres): forward of the car's centre, to each side of its axis, above the ground */
enum { CONE_LAMP_FORWARD_CM = 220, CONE_LAMP_LATERAL_CM = 70, CONE_LAMP_HEIGHT_CM = 65 };

/* The beam's gain at `lift` (0..256) of the beam: floor + (256 - floor) x lift / 256, the gain a node takes at that lift. */
int headlight_pool_gain_at_lift(int floor, int lift);

#endif
