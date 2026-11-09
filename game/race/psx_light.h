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

enum { PSX_LIGHT_SNOW_SLOTS = 144 };   /* TRACK_TEXTURE_SLOTS (track_art.h), checked in track_light_file.c */

/* port (track light file, `tone`): a scene tone a track's stretch takes: a colour and how strongly (0..100) the stretch's polygons are seen through it. At most PSX_LIGHT_TONES distinct ones a track. */
enum { PSX_LIGHT_TONES = 16, PSX_TONE_BASE_BITS = 3 };   /* a vertex class's scene tone field is 5 bits: 1..16 the tones */

/* port (track light file, `lamp`): a street lamp as light: where it stands, how far it reaches and the colour it adds (the colour already times its percent / 100: 1 is a lamp at 100 percent) */
typedef struct { uint32_t node; float x, z, reach; float rgb[3]; } PsxLightLamp;
enum { PSX_LIGHT_LAMP_RADIUS = 5 };   /* the nodes a lamp with no radius of its own reaches */
/* A slot is a scene tone (look 0: `rgb` and `percent`, the colour the stretch's polygons are seen through) or, shared with them, a tunnel zone's own look (look 1: `light`, percent of full light, and
 * `cast`, 0..1 a channel, as the time's tunnel_tone: a tunnel zone whose look is not the track's). No padding: slots are compared and copied as bytes. */
typedef struct { uint8_t rgb[3]; uint8_t percent; uint8_t look; uint8_t pad[3]; float light; float cast[3]; } PsxLightTone;

typedef struct {
    uint8_t *data;      /* nodes * PSX_LIGHT_POINTS bytes (owned), NULL when none is loaded */
    uint32_t nodes;
    PsxLgs script;      /* the track's light script (psx_light_set_script), read when `scripted` */
    int scripted;
    HeadlightPool *pool; /* port (psx_headlights): the cars' pools of light the night's nodes are lifted by (headlight_pool.h); NULL none (not owned: the one who set it
                         * keeps the nodes up to date; the light reads it through a const PsxLight) */
    uint8_t *hall;      /* port (made light): per node (owned), 1 where the node is a lit hall's (psx_light_synth.h: the car takes the lamps' alternation there); NULL none */
    uint8_t *tunnel;    /* port (made_tunnels): 1 per node the road marks a tunnel (owned), the made light's; NULL none (then the rows decide: psx_light_tunnel) */
    int city_tunnel;    /* port (made_tunnels): 1 CY1, 2 CY2 and CY3, 3 TR3: the road tunnels' tables take the CD's warm cast and the light it needs (ported_view.c) */
    int look_set;       /* port (track light file): 1 the file gives the tunnels' look: look_light (percent of full light: 100 is row 255, at any time of day) and look_cast (the colour, 0..1 a channel); 0 the made light's own (warm_*, neutral_cast) */
    double look_light;
    float look_cast[3];
    float warm_cast[2][3];   /* port (made_tunnels): city_tunnel's tunnel look [0 day, 1 night]: the cast of the dim levels (red, green, blue) ... */
    int warm_percent[2];     /* ... and its light, the gain over the made tunnel's own in percent (psx_light_synth_warm_look; 0 none) */
    int neutral_cast;   /* port (made_tunnels, i074): 1 where the road tunnels' tables keep the palette's own hue, no olive-orange cast (Burnt Sienna's mine is blue); 0 the cast */
    uint8_t *canyon;    /* port (canyon_shade): per node (owned), 1 where TR6's property-16 stretch (the canyon) is; NULL none */
    uint8_t *car_dim;   /* port (parking_dark): per node (owned), 1 where the car keeps the darkest tone of the lamp alternation with no alternating (TR7's parking); NULL none */
    int forest_road;    /* port (forest_neutral): 1 and the file's nodes forest_from..forest_to, which the rows mark as a tunnel (a forest's shade), are drawn with the road's tables, not a tunnel's cast */
    uint32_t forest_from, forest_to;
    int shoulder_road;  /* port (shoulder_light): the shoulder's far edge (points 2 and 7) takes at least the road's light (1 or 6) before the beam and the lamps scale it, in the open night; 0 its own row */
    int lamp_bright;    /* port (lamp_pool_bright): a street lamp's pool (car_pool's classes) is not dimmed by the night's dark floor; 0 it is */
    int night_lift;     /* port (city_lift, vegas_lift): 0..256, how far toward full the open night's dark floor is lifted (0 none: the pools' own floor) */
    int beam_on_top;    /* port (city_lift 3): with night_lift the beam's gain is taken on top of the lifted floor, not as the larger of the two; 0 the larger (city_lift 1 and 2, vegas_lift) */
    int lamp_local;     /* port (city_lift 3): a street lamp's floor (psx_light_lamp_floor) falls off steeply with the pool's class, full only at the lamp (class 4), so the pools are local and the road between lamps keeps the dark base for the beams; 0 linear in the class */
    int beam_open;      /* port (city_lift 3): the light out of 255 the headlight beam gives the open ground at its full, where the night's own rows are darker (the City's are about 98); 0 the beam only scales the row's light */
    int tunnel_floor;   /* port (city_lift 3): a tunnel's dark floor (of 256) in place of TUNNEL_FLOOR, so its lamps stand out against a darker open night; 0 TUNNEL_FLOOR */
    uint8_t object_floor;   /* port (vegas_objects): the least light (of 255) a 3D object (a building, a sign) takes from its node; 0 none, the node's own light */
    int gate_board;     /* port (finish_gate_light): the stage gate's scenery board plus one, which keeps full light at night (finish_gate_light.h); 0 none */
    int beam_tunnel;    /* port (tunnel_headlight): the light out of 255 the headlight beam gives the walls and road of a tunnel or a lit hall at night (180 on); 0 the beam as it was (TUNNEL_BEAM in a tunnel, nothing in a hall) */
    uint8_t *pool_class; /* port (lamp_pool_tint): per node (owned), 0 none, 1..4 how strong a street lamp's pool is there (psx_light_lamp_classes); NULL none */
    uint8_t snow_map[PSX_LIGHT_SNOW_SLOTS];   /* port (snow_weather): the places file's `snow_map from to` lines: texture slot `from` is drawn with slot `to`'s bitmaps in the rally (track_art_remap_slots); to + 1, 0 none */
    int snow;           /* port (snow_weather): the places file's `snow 1`: the track's rain is snow (weather_snow.h), and its rally road is covered in it (rally_gravel.h) */
    int from_places;    /* port (tunnel_dry): the tunnel / canyon / car_dim arrays are a places file's (track_light_place_parse): the only source of the closed zones, the road's bits are not added to them */
    int from_file;      /* port (track light file): the light is a file's, whatever the port's flags: its canyon and tunnel nodes count without canyon_shade / car_tunnel_light on */
    PsxLightTone tone[PSX_LIGHT_TONES];   /* port (`tone`): the track's scene tones, tone_count of them */
    int tone_count;
    uint8_t *tone_id;   /* per node (owned): the node's tone's number plus one (1..tone_count), 0 none; NULL no tone at all */
    uint16_t *tone_mask;/* per node (owned): the points of the node's row the tone is on (bit p: p0..p10), all of them for a stretch with no strip */
    uint8_t *car_tone;  /* port (track light file, tono_coche): per node 4 bytes (owned), the car's tone there: red, green, blue and its intensity 0..100; NULL none (psx_car_tone_table) */
    uint8_t *car_pool;  /* port (lamp_car_shade): per node (owned), 0 none, 1..4 how strong a street lamp's pool is under a car there (the new node's); NULL none */
    uint32_t *lamp_node;    /* port (track light file export): the nodes of the street lamps the pools were made from (owned), lamp_node_count of them, and whether the track is a ring; the export writes them as `lamp` lines */
    int lamp_node_count, lamp_ring;
    PsxLightLamp *lamps;    /* port (track light file, `lamp`): the file's lamps, by node (owned), lamp_n of them; NULL none. Their x / z / reach are set by psx_light_lamps_place */
    uint8_t *lamp_lift;     /* port (`lamp`): per row point (nodes * PSX_LIGHT_POINTS, owned), how much the lamps lift it, 0..255 (the baked pools: the open night's floor rises by it); NULL none */
    int lamp_n, lamp_placed, lamp_reach_nodes;   /* lamp_placed: x / z / reach are in metres; lamp_reach_nodes: the widest lamp's radius in nodes */
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

/* port (tunnel look): 1 when `node` is in a tunnel of a light that has a look (look_set): its road can be lit above full light (a tunnel_tone over 100), so what is drawn on the road (the car's shadow) is
 * made in the shown palette. 0 otherwise. */
int psx_light_look_tunnel(const PsxLight *light, uint32_t node);

/* port (weather_zones): 1 when `node` is a closed zone for the weather (no rain, fog or lightning inside): the file's `tunnel` nodes (a made light's road tunnels) and its halls (a tunnel with a lot of light), or the rows' tunnel
 * where the light has no tunnel array. A node the light marks `canyon` is open air. */
int psx_light_weather_closed(const PsxLight *light, uint32_t node);

/* port (tunnel zone looks): the light the road gets at `node` (the game's node), 0..1: the mean of the row's road points (0, 1 and 6) as the world takes them (a tunnel's road is capped at
 * PSX_LIGHT_TUNNEL_ROAD of 255), without the headlights' pools; 1 with no rows. A car in a tunnel is never brighter than this. */
float psx_light_road_light(const PsxLight *light, uint32_t node);

/* port (tunnel zone looks): the look of the tunnel at file node `at` (the node as the file numbers it, 0..nodes - 1): the light (percent of full light) and the colour its zone gives, or the time's
 * tunnel_tone when the zone has none of its own. 0 (outputs untouched) when the light has no look (look_set 0). */
int psx_light_zone_look_at(const PsxLight *light, uint32_t at, double *percent, float cast[3]);

/* port (track light file): how the polygons of a tunnel are tabled for the time whose ambient tint is `shown` (drive_light_tint): the tables' tint (three equal channels in a tunnel's own light), the
 * cast of its darkened levels and the least light, or no cast at all (`cast_on` 0: the open road's tables). `lit_tint` is the open road's. `lamp_count`: the CD's lamps the track has. The look is the file's
 * (look_set: the light's percent is of full light, so the gain is it over the time's own tint) else the made light's (psx_light_synth_warm_look, the mine's, the olive default). */
typedef struct { int cast_on; float tint[3]; float cast[3]; float min_light; } PsxTunnelLook;
void psx_light_tunnel_look(const PsxLight *light, const float shown[3], const float lit_tint[3], int lamp_count, PsxTunnelLook *out);

/* port (`tone`): the class of the point `point` (a row's point as psx_light_at takes it) of node `node`: psx_light_tunnel_class in the low PSX_TONE_BASE_BITS bits and the point's tone (1..PSX_LIGHT_TONES, 0 none) above them */
int psx_light_vertex_class(const PsxLight *light, uint32_t node, int point);

/* port (`lamp`): a lamp's light at a distance: (1 - distance / reach)^2 under reach, else 0: full under the lamp, smooth to nothing at the reach, no step anywhere */
float psx_light_lamp_falloff(float distance, float reach);

/* port (`lamp`): the lamps within reach of the four nodes of a polygon (the track as it runs; PSX_LIGHT_NO_NODE ones count for none): their indexes into `lamps`, at most `max`. Returns how many. */
int psx_light_lamps_near(const PsxLight *light, const uint32_t node[4], int out[], int max);

/* port (`lamp`): the light of the lamps `idx` (n of them) at the ground point x, z (metres): the sum of their strengths times their falloffs (0 none), and in `rgb` their colour (the mean weighted by what each gives, 0..1 a channel). */
float psx_light_lamp_at(const PsxLight *light, const int idx[], int n, double x, double z, float rgb[3]);

/* port (`lamp`): a car at ground point x, z (node `node`) lit by the lamps: their colour times LAMP_CAR_ADD added to the ratio `out` (never darkened, to the ceiling the old pool had): 1 when it changed. */
#define LAMP_ADD 0.4f        /* what a lamp at 100 percent adds under it, as a share of full (255) */
#define LAMP_CAR_ADD 0.6f
#define LAMP_BAKE 1.0f       /* how far toward full light a lamp at 100 percent lifts the rows and the night's floor under it */
int psx_light_lamp_car(const PsxLight *light, uint32_t node, double x, double z, float out[3]);

/* port (`lamp`): the light level `level` of an object at the ground point x, z (metres) beside node `node` (its terrain's mean, as psx_light_node), raised to what the lamps' pool gives there. The rows' bake lifts the
 * terrain points of the node by the lamps' falloff at each point's own place (lamp_lift), and an object takes their mean: where the object's own place is lit more than that mean (a building nearer the lamp than the
 * verge's points are), the level rises by the difference, as the bake would have put it (the same toward-full lift, not added twice). Unchanged with no lamps, no bake (lamp_lift NULL), or where the lamps give the
 * object no more than its terrain's. */
uint8_t psx_light_object_lamp(const PsxLight *light, uint32_t node, double x, double z, uint8_t level);

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
