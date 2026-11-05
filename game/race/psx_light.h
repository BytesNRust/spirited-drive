/* port: the PS1 look's baked light (re/notes/psx-lgt-format.md, branch docs/psx-lgt). The PS1 CD has, for each track and time
 * of day (A, B, C), a Z<TRACK><A|B|C>.LGT: u32 node count, u32 scale, 11 bytes of intensity a node (0 black, 255 full)
 * in the order of the node's terrain row (TriRow: middle, right[0..4], left[0..4]), then a 12-byte CRCF trailer. The road's
 * points never darken; the terrain under a bridge does. In a tunnel a 0 beyond a lit wall foot (point 2 / 7 at PSX_LIGHT_WALL_FOOT or more) is
 * no data, not black: psx_light_at gives it PSX_LIGHT_WALL, about half light, as the PS1's tunnel walls, pillars and ceiling look. The bytes are the user's own files, read
 * at the race's start; nothing of them is in the repo, and the tests use made-up rows. */
#ifndef TNFS_PSX_LIGHT_H
#define TNFS_PSX_LIGHT_H

#include "game/race/headlight_pool.h"
#include "game/race/psx_lgs.h"

#include <stddef.h>
#include <stdint.h>

enum { PSX_LIGHT_POINTS = 11, PSX_LIGHT_FULL = 255, PSX_LIGHT_WALL = 128, PSX_LIGHT_WALL_FOOT = 64, PSX_LIGHT_TUNNEL_ROAD = 100 };

typedef struct {
    uint8_t *data;      /* nodes * PSX_LIGHT_POINTS bytes (owned), NULL when none is loaded */
    uint32_t nodes;
    PsxLgs script;      /* the track's light script (psx_light_set_script), read when `scripted` */
    int scripted;
    HeadlightPool *pool; /* port (psx_headlights): the cars' pools of light the night's nodes are lifted by (headlight_pool.h); NULL none (not owned: the one who set it
                         * keeps the nodes up to date; the light reads it through a const PsxLight) */
    uint8_t *hall;      /* port (made light): per node (owned), 1 where the node is a lit hall's (psx_light_synth.h: the car takes the lamps' alternation there); NULL none */
    uint8_t *tunnel;    /* port (made_tunnels): 1 per node the road marks a tunnel (owned), the made light's; NULL none (then the rows decide: psx_light_tunnel) */
    int neutral_cast;   /* port (made_tunnels, i074): 1 where the road tunnels' tables keep the palette's own hue, no olive-orange cast (Burnt Sienna's mine is blue); 0 the cast */
    uint8_t *canyon;    /* port (canyon_shade): per node (owned), 1 where TR6's property-16 stretch (the canyon) is; NULL none */
    uint8_t *car_dim;   /* port (parking_dark): per node (owned), 1 where the car keeps the darkest tone of the lamp alternation with no alternating (TR7's parking); NULL none */
    int forest_road;    /* port (forest_neutral): 1 and the file's nodes forest_from..forest_to, which the rows mark as a tunnel (a forest's shade), are drawn with the road's tables, not a tunnel's cast */
    uint32_t forest_from, forest_to;
    int lamp_bright;    /* port (lamp_pool_bright): a street lamp's pool (car_pool's classes) is not dimmed by the night's dark floor; 0 it is */
    int night_lift;     /* port (city_lift, vegas_lift): 0..256, how far toward full the open night's dark floor is lifted (0 none: the pools' own floor) */
    uint8_t object_floor;   /* port (vegas_objects): the least light (of 255) a 3D object (a building, a sign) takes from its node; 0 none, the node's own light */
    int gate_board;     /* port (finish_gate_light): the stage gate's scenery board plus one, which keeps full light at night (finish_gate_light.h); 0 none */
    int beam_tunnel;    /* port (tunnel_headlight): the light out of 255 the headlight beam gives the walls and road of a tunnel or a lit hall at night (180 on); 0 the beam as it was (TUNNEL_BEAM in a tunnel, nothing in a hall) */
    uint8_t *pool_class; /* port (lamp_pool_tint): per node (owned), 0 none, 1..4 how strong a street lamp's pool is there (psx_light_lamp_classes); NULL none */
    uint8_t *car_pool;  /* port (lamp_car_shade): per node (owned), 0 none, 1..4 how strong a street lamp's pool is under a car there (the new node's); NULL none */
    int pooled;         /* port (lamp_pools): the street lamps' pools are in `data` already (psx_light_add_pools) */
    int reversed;       /* port: the track runs the other way round (tri_reverse.h): a node and a side are asked as the new file has them, the
                         * file's are the old ones (node j is the file's (nodes - j) mod nodes, the two chains of a row swapped). Last. */
} PsxLight;

/* The file's bytes as a PsxLight: its node count must be `expected_nodes` (the track's ring nodes) and the file at least as
 * long as the rows. 1 on success; `out` is untouched otherwise. */
int psx_light_parse(const uint8_t *file, size_t size, uint32_t expected_nodes, PsxLight *out);

/* The track's light script (the LGS, copied): the points' light is then the exe's (psx_lgs_light, by block and node parity, as grey: the luminance of the
 * three channels, 0..255 with 255 the lit road), the tunnels are the script's RAMP contexts, and the fitted rules below are not used. Without one the
 * LGT's bytes are read as they are and the fitted rules stand. 1 on success. */
int psx_light_set_script(PsxLight *light, const PsxLgs *script);

/* The LGT byte of a point as the file has it (the node taken modulo the nodes, the last node as the one before it; PSX_LIGHT_FULL out of range). */
uint8_t psx_light_byte(const PsxLight *light, uint32_t node, int point);

void psx_light_free(PsxLight *light);

/* port: the light as a reversed track asks it (see `reversed`). 1 on success. */
int psx_light_set_reversed(PsxLight *light, int reversed);

/* The node of the file's (and the script's) own direction that node `node` of the track as it runs is: `node` modulo the nodes, mirrored when reversed. */
uint32_t psx_light_old_node(const PsxLight *light, uint32_t node);

/* 1 when node `node` is in a tunnel: in a script's RAMP context, or without a script its row is a tunnel's: a lit wall foot (point 2 or 7, PSX_LIGHT_WALL_FOOT or more) with 0s from point 4 / 9 on, either side.
 * Its road is dimmed (PSX_LIGHT_TUNNEL_ROAD), its wall foot capped (PSX_LIGHT_WALL) and its darkened light cast (the renderer). 0 without data. */
int psx_light_tunnel(const PsxLight *light, uint32_t node);

/* port (lamp_pool_tint): what the light of the polygons at `node` is tabled by: 1 a tunnel (psx_light_tunnel), 2..5 a street lamp's pool of class 1..4 (the road there takes a warm tint), 0 else. */
int psx_light_tunnel_class(const PsxLight *light, uint32_t node);

/* port (lamp_car_shade): a car's light ratio (`out`, 0..1 a channel) times a warm lamp gain by the pool at `node` (car_pool, 2.6 / 2.4 / 1.8 at the strongest, fading with its class, at most 2, never darkened): 1 when it
 * changed, 0 with no pool there. */
int psx_light_car_pool(const PsxLight *light, uint32_t node, float out[3]);

/* The intensity of terrain-row point `point` (0..10, TriRow order) of node `node` (taken modulo the nodes); the file's last
 * node is not light (random bytes) and takes the one before it; PSX_LIGHT_FULL without data or for a point out of range. */
uint8_t psx_light_at(const PsxLight *light, uint32_t node, int point);

/* port (headlight_cone): the same for a point of the row at the ground place xz = { x, z } in metres (NULL: as psx_light_at, the node's own place): with the pool's cone on the beam is the
 * cone's at that place, so the light changes inside a polygon's corners and not a node's row at a time. */
uint8_t psx_light_at_xz(const PsxLight *light, uint32_t node, int point, const double *xz);

/* The light of what stands at a node (a scenery object): the mean of its terrain points' intensities, the road's left out;
 * PSX_LIGHT_FULL without data. */
uint8_t psx_light_node(const PsxLight *light, uint32_t node);

/* port (lamp_pool_bright): the floor (0..256, 0 none) a street lamp's pool lifts a node to (0 for PSX_LIGHT_NO_NODE); and the open night's gain (0..256) at a beam lift 0..256 with such a floor: what
 * psx_light_at multiplies a row's byte by outside a tunnel, the dark's floor and the night lift in it (256 without a pool) */
int psx_light_lamp_floor(const PsxLight *light, uint32_t node);
int psx_light_open_gain_floor(const PsxLight *light, int lift, int lamp_floor);

/* a node that is not known (a polygon drawn by a pass that has none): no street lamp's floor is read for it, the open night's is used */
#define PSX_LIGHT_NO_NODE 0xffffffffu

/* port (headlight_cone): the light of `count` points of a built road, each at its own ground place (world: 16.16 metres x y z; node and point: the node and the place in its row of each, as the build
 * keeps them), into out[]; the follow camera's relight. Only with a cone pool: without one (NULL pool or cone 0) out[] is left as it is. */
void psx_light_relight_xz(const PsxLight *light, const int32_t (*world)[3], const uint32_t *node, const uint8_t *point, int32_t count, uint8_t *out);

/* port (headlight_cone): the same for an object that stands at the ground place xz = { x, z } in metres (NULL: as psx_light_node): the cone's light at its own place */
uint8_t psx_light_node_xz(const PsxLight *light, uint32_t node, const double *xz);

#endif
