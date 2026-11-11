/* port (the light effects' light on the cars): the track's light as the cars take it taken out of ported_view.c: the ambient by segment, the road's tunnels and the rain's roofs, the headlights' cars and cone,
 * the light a car is drawn in (car_light, car_table). ported_view.c sets it and asks; what this file takes of the view is in view_shared.h. */

#include "game/race/lamp_emissive.h"
#include "game/race/light_tint.h"
#include "game/race/police_lights.h"
#include "game/race/light_synth.h"
#include "game/sys/port_settings.h"
#include "viewer/drive_light.h"
#include "viewer/view_light.h"
#include "viewer/view_shared.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* port (the light effects' light): the track's light, or NULL for none (the frames are then the exe's, byte for byte) */
const TrackNodeLight *light_effects;   /* the track's light (set below) */
int light_changed;   /* the light editor's reload put new rows in the same light: the built track is built again */

void ported_view_light_changed(void) { light_changed = 1; }

void ported_view_set_light(const TrackNodeLight *light, const float tint[3])
{
    light_effects = light;
    for (int k = 0; k < 3; k++) lit_tint[k] = light != NULL && tint != NULL ? tint[k] : 1.0f;
    lit_tinted = lit_tint[0] != 1.0f || lit_tint[1] != 1.0f || lit_tint[2] != 1.0f;
}

/* port (the baked light on the cars): the ambient colour by segment (a node is 4 to a segment) over the world's tint is the ratio each car's
 * colours are scaled by, as a table by the nearest palette entry; the ratio is kept to 1/16 and the tables cached by it */
enum { AMBIENT_TABLES = 8, AMBIENT_STEP = 16 };
static PsxLgs ambient;
static int ambient_on;
static int synth_on;   /* the light is one light_synth made: the cars and the dash take its tunnel factor */
static uint32_t ambient_nodes;
static float ambient_tint[3] = { 1, 1, 1 };

void ported_view_set_light_ambient(const PsxLgs *lgs, uint32_t ring_nodes, const float tint[3])
{
    ambient_on = lgs != NULL && tint != NULL && ring_nodes >= 4;
    if (!ambient_on) return;
    ambient = *lgs, ambient_nodes = ring_nodes;
    for (int k = 0; k < 3; k++) ambient_tint[k] = tint[k];
}


void ported_view_set_light_synth(int on) { synth_on = on; }

/* port (the baked light on the cars and the dash): the exe's light of the road's centre at the car's own node (light_tint_car_ratio; the exe gives every car the
 * viewer's, the port its own node's), the one factor of the cars and the dash; 0 and `out` untouched when there is no light script. A context's colour is
 * its own tunnel's tint, and a RAMP context's odd / even nodes are the lamps' alternation. */
static int ratio_of(int32_t node, float out[3]);

/* port (headlight_beams): the nodes the road itself marks a tunnel (its property bit, runs of LIGHT_SYNTH_MIN_RUN and more: the wooden tunnels and sheds the CD's light
 * rows do not all mark), made once for the track; NULL when it has no data */
/* two callers ask with different counts (the light script's and the ring's): a slot each, or they would remake the flags every call, every frame;
 * dropped when the track is forgotten (the next track's race may sit at the same address with as many nodes) */
enum { TUNNEL_SLOTS = 2 };
static struct { uint8_t *flags; uint32_t have; const RaceState *race; } tunnel_slot[TUNNEL_SLOTS];
static int tunnel_next;

static uint8_t *rain_cover_flags;   /* rain_cover, below */
void road_tunnels_forget(void)
{
    free(rain_cover_flags);
    rain_cover_flags = NULL;
    for (int i = 0; i < TUNNEL_SLOTS; i++) free(tunnel_slot[i].flags), tunnel_slot[i].flags = NULL;
}

const uint8_t *road_tunnels(const RaceState *race, uint32_t nodes)
{
    if (nodes == 0) return NULL;
    for (int i = 0; i < TUNNEL_SLOTS; i++)
        if (tunnel_slot[i].flags != NULL && tunnel_slot[i].have == nodes && tunnel_slot[i].race == race) return tunnel_slot[i].flags;
    const int at = tunnel_next;
    tunnel_next = (tunnel_next + 1) % TUNNEL_SLOTS;
    free(tunnel_slot[at].flags);
    tunnel_slot[at].flags = NULL;
    uint8_t *flags = malloc(nodes);
    uint8_t *property = malloc(nodes);
    if (flags == NULL || property == NULL) { free(property); free(flags); return NULL; }
    for (uint32_t i = 0; i < nodes; i++) property[i] = race_node(race, (int32_t)i).property;
    light_synth_tunnels(property, nodes, 1, flags);
    free(property);
    tunnel_slot[at].flags = flags, tunnel_slot[at].have = nodes, tunnel_slot[at].race = race;
    return flags;
}

/* port (rain_fx): the nodes with a roof over the road for the rain (light_synth_rain_cover), made once for the track like road_tunnels' */
static uint32_t rain_cover_have;
static const RaceState *rain_cover_race;

const uint8_t *rain_cover(const RaceState *race, uint32_t nodes)
{
    if (nodes == 0) return NULL;
    if (rain_cover_flags != NULL && rain_cover_have == nodes && rain_cover_race == race) return rain_cover_flags;
    free(rain_cover_flags);
    rain_cover_flags = malloc(nodes);
    uint8_t *property = malloc(nodes);
    if (rain_cover_flags == NULL || property == NULL) { free(property); free(rain_cover_flags); rain_cover_flags = NULL; return NULL; }
    for (uint32_t i = 0; i < nodes; i++) property[i] = race_node(race, (int32_t)i).property;
    light_synth_rain_cover(property, nodes, 1, rain_cover_flags);
    free(property);
    rain_cover_have = nodes, rain_cover_race = race;
    return rain_cover_flags;
}

/* how far the car is on from its node to the next, out of 256 (its place projected on the road's step) */
static uint8_t headlight_frac(const RaceState *race, const Car *car)
{
    const TriNode a = race_node(race, car->node), b = race_node(race, car->node + 1);
    const double sx = (double)b.position.x - a.position.x, sz = (double)b.position.z - a.position.z;
    const double len2 = sx * sx + sz * sz;
    if (len2 <= 0.0) return 0;
    const double t = (((double)car->x - a.position.x) * sx + ((double)car->z - a.position.z) * sz) / len2;
    return (uint8_t)(t <= 0.0 ? 0 : t >= 1.0 ? 255 : t * 256.0);
}

/* port (headlight_cone): the nodes' centres (x, z in 16.16 metres), once per track: what the objects and the cars, lit by the node, take the cones at */
static const int32_t *cone_node_xz(const RaceState *race, uint32_t nodes)
{
    static int32_t *xz;
    static uint32_t have;
    static const RaceState *for_race;
    if (nodes == 0) return NULL;
    if (xz != NULL && have == nodes && for_race == race) return xz;
    free(xz);
    xz = malloc(sizeof *xz * 2 * nodes);
    if (xz == NULL) return NULL;
    for (uint32_t i = 0; i < nodes; i++) {
        const TriNode n = race_node(race, (int32_t)i);
        xz[2 * i] = n.position.x, xz[2 * i + 1] = n.position.z;
    }
    have = nodes, for_race = race;
    return xz;
}

/* port (headlight_cone): a car's heading as a unit vector on the ground from its yaw (24 bits a turn) */
static void cone_heading(const Car *car, double *hx, double *hz)
{
    const double a = (double)(car->yaw & 0xffffff) * (6.283185307179586 / 16777216.0);
    *hx = sin(a), *hz = cos(a);
}

/* port (headlight_beams): the tick each car's body was last drawn (its draw distance, the render scale's): a car that is not drawn does not light the road, so no lit road
 * shows far from a car that is not there yet. Stored as tick + 1: 0 is never */
int32_t body_drawn_tick[RACE_CARS];
HeadlightPool *pool_fed;   /* the pool headlights_update last fed: its tunnel flags and node table are freed with the track, so it lets go of them then */

/* port (headlight_beams): the cars with their lights on, each frame - the active cars of the race, the player first - are the pools the road is lit in */
void headlights_update(void)
{
    if (light_effects == NULL || light_effects->pool == NULL) return;
    HeadlightPool *pool = light_effects->pool;
    pool_fed = pool;
    static int32_t seen[RACE_CARS];
    static int8_t travel[RACE_CARS];
    static uint32_t of_ring;
    pool->count = 0;
    pool->ring = light_effects->nodes;
    reflect_lamps_on = 0;   /* port (reflectors): only the player's lamps */
    if (shadow_race == NULL) return;
    const int32_t ring = (int32_t)pool->ring;
    if (of_ring != pool->ring) of_ring = pool->ring, memset(seen, 0, sizeof seen), memset(travel, 0, sizeof travel);   /* another track */
    pool->tunnel = road_tunnels(shadow_race, light_effects->nodes);
    pool->cone = port_headlight_cone();
    pool->node_xz = pool->cone ? cone_node_xz(shadow_race, light_effects->nodes) : NULL;
    const int32_t n = shadow_race->car_count < RACE_CARS ? shadow_race->car_count : RACE_CARS;
    for (int32_t i = 0; i < n && pool->count < HEADLIGHT_MAX; i++) {
        const Car *car = shadow_race->cars[i];
        if (car == NULL || !(car->flags_51a & 4)) continue;
        if (i != race_slot(shadow_race->player)) {
            const int drawn = !(body_drawn_tick[i] == 0 || shadow_race->tick + 1 - body_drawn_tick[i] > 4 || shadow_race->tick + 1 < body_drawn_tick[i]);
            const Car *const player = shadow_race->cars[race_slot(shadow_race->player)];
            int32_t behind = player != NULL ? player->node - car->node : -1;
            if (ring > 0) behind = behind > ring / 2 ? behind - ring : behind < -ring / 2 ? behind + ring : behind;
            if (!headlight_car_lights(drawn, behind, pool->cone)) continue;   /* not drawn lately and not just behind the camera (headlight_cone): no light */
        }
        int32_t moved = car->node - seen[i];   /* which way the car travels: the nodes it moved over, the gear on the first frame */
        if (ring > 0) moved = moved > ring / 2 ? moved - ring : moved < -ring / 2 ? moved + ring : moved;
        if (travel[i] == 0) travel[i] = car->gear == LAMP_REVERSE_GEAR ? -1 : 1;
        if (moved != 0 && moved > -4 && moved < 4) travel[i] = moved > 0 ? 1 : -1;
        seen[i] = car->node;
        if (pool->cone) {   /* port (headlight_cone): the cone from the car's place along its nose */
            pool->cone_x[pool->count] = car->x / 65536.0, pool->cone_z[pool->count] = car->z / 65536.0;
            cone_heading(car, &pool->cone_hx[pool->count], &pool->cone_hz[pool->count]);
        }
        if (i == race_slot(shadow_race->player)) {   /* port (reflectors) */
            double hx, hz;
            cone_heading(car, &hx, &hz);
            reflect_lamps_on = 1, reflect_x = car->x / 65536.0, reflect_z = car->z / 65536.0, reflect_hx = hx, reflect_hz = hz;
            reflect_dark = drive_light_night() || headlight_pool_tunnel(pool, (uint32_t)car->node);
        }
        pool->cone_car[pool->count] = (int8_t)i;
        pool->sub[pool->count] = headlight_frac(shadow_race, car);
        pool->dir[pool->count] = travel[i];
        pool->node[pool->count++] = car->node;
    }
}

/* xz: the car's own place on the ground in metres, or NULL (the node's) */
static int ratio_at_xz(int32_t node, const double *xz, int car, float out[3])
{
    int found = ratio_of(node, out);
    found = headlight_pool_car_ratio_start(found, light_effects != NULL && light_effects->pool != NULL, port_car_night_lift(), out);
    /* port (car_night_lift): a light with no ratio for the car (no ambient, no synth: CL1's night) left `out` unset, and car_light made the car's table from 0: every texel but the paint's black, a wheel a flat disc. The car's own light is full, and the pool's gain below lights it as it lights the road */
    /* port (car_tunnel_match): a made tunnel with a look keeps the light its look gives (the world's tables are not scaled by the headlight pool's floor there, light_at_dark), so the car is not either: the pool's share took a further 60 % off the car */
    const int match_tunnel = found && port_car_tunnel_match() >= 1 && light_effects != NULL && light_effects->look_set && track_node_light_tunnel(light_effects, light_tint_ring_node(node, light_effects->nodes));
    if (found && !match_tunnel && light_effects != NULL && light_effects->pool != NULL) {   /* port (headlight_beams): a car in the dark or in a pool takes its light, as the road under it does */
        const uint32_t at = light_tint_ring_node(node, light_effects->nodes);
        int g;
        if (light_effects->pool->cone && xz != NULL) {   /* port (headlight_cone): by where the car is in every car's patch of light, its own included (the glow round it): rivals, police and the player alike */
            const int lift = headlight_pool_car_lift_xz(light_effects->pool, car, xz[0], xz[1]);   /* its own patch limited, the others' added (i097) */
            g = headlight_pool_car_gain(headlight_pool_gain_at_lift(track_node_light_tunnel(light_effects, at) ? TUNNEL_FLOOR : HEADLIGHT_FLOOR, lift));
        } else {
            g = headlight_pool_car_at(light_effects->pool, at) ? 256 : headlight_pool_car_gain(track_node_light_tunnel(light_effects, at) ? headlight_pool_gain_tunnel(light_effects->pool, at) : headlight_pool_gain(light_effects->pool, at));   /* a car with its lights on keeps its own light: the beam is ahead of it */
        }
        g = headlight_pool_car_night_lift(g, port_car_night_lift());   /* port (car_night_lift) */
        const float gain = (float)g / 256.0f;
        for (int k = 0; k < 3; k++) out[k] *= gain;
        if (port_lamp_car_shade()) (void)track_node_light_car_pool(light_effects, at, out);   /* port (lamp_car_shade): and the street lamp's pool lights it */
    }
    if (!found && light_effects != NULL && light_effects->car_pool != NULL && port_lamp_car_shade()) {   /* port (lamp_car_shade): a made light with no tunnels gives the car no ratio, so the lamps' pools had nothing to lift: the car's own is neutral, the pool's warm gain on it */
        static int mode = -1;
        if (mode < 0) mode = getenv("TNFS_LAMP_CAR_MODE") ? atoi(getenv("TNFS_LAMP_CAR_MODE")) : 1;
        const uint32_t at = light_tint_ring_node(node, light_effects->nodes);
        out[0] = out[1] = out[2] = 1.0f;
        if (mode == 2) {   /* steady: the nearest lamp is within 8 nodes either side, the lift is the weakest pool's all the way along the lamps' stretch */
            int near = 0;
            for (int d = -8; d <= 8 && !near; d++) near = light_effects->car_pool[light_tint_ring_node((int32_t)at + d, light_effects->nodes)] != 0;
            if (near) {
                TrackNodeLight steady = *light_effects;
                uint8_t keep = 2;
                steady.car_pool = &keep;
                steady.nodes = 1;
                return track_node_light_car_pool(&steady, 0, out);
            }
            return 0;
        }
        return track_node_light_car_pool(light_effects, at, out);
    }
    if (light_effects != NULL && light_effects->lamp_n > 0 && xz != NULL) {   /* port (`lamp`): the street lamps add their light to the car by its distance to each */
        float lit[3] = { found ? out[0] : 1.0f, found ? out[1] : 1.0f, found ? out[2] : 1.0f };
        if (track_node_light_lamp_car(light_effects, light_tint_ring_node(node, light_effects->nodes), xz[0], xz[1], lit)) {
            memcpy(out, lit, sizeof lit);
            return 1;
        }
    }
    return found;
}

/* a car's ratio at its own place */
int ratio_of_car(const Car *car, float out[3])
{
    const double xz[2] = { car->x / 65536.0, car->z / 65536.0 };
    return ratio_at_xz(car->node, xz, car->index, out);
}

static const float CANYON_SHADE_FACTOR = 0.70f;   /* port (canyon_shade): the owner's pick, 30 % darker */

static int ratio_of(int32_t node, float out[3])
{
    if (synth_on && light_effects != NULL) {   /* port: a light made from the road (light_synth.h): no script, the tunnel's lamp alternation */
        const uint32_t at = light_tint_ring_node(node, light_effects->nodes);
        if (port_car_tunnel_light() || light_effects->from_file) light_synth_car_tunnel_match(light_effects, at, port_car_tunnel_match(), (lit_tint[0] + lit_tint[1] + lit_tint[2]) / 3.0f, out);   /* port (car_tunnel_light, car_tunnel_match) */
        else light_synth_car(light_effects, at, out);
        if (light_effects->canyon != NULL && light_effects->canyon[light_tint_ring_node(node, light_effects->nodes)]) {   /* port (canyon_shade): the walls shade the car in the canyon */
            const float f = port_canyon_shade() || light_effects->from_file ? CANYON_SHADE_FACTOR : 1.0f;
            for (int k = 0; k < 3; k++) out[k] *= f;
        }
        return 1;
    }
    if (!ambient_on || light_effects == NULL) return 0;
    const int32_t at = (int32_t)light_tint_ring_node(node, ambient_nodes);
    /* the script's blocks are of the file's direction: a reversed track asks them at the node's old place */
    light_tint_car_ratio(&ambient, ambient_nodes, (int32_t)track_node_light_old_node(light_effects, (uint32_t)at),
                      track_node_light_byte(light_effects, (uint32_t)at, 0), AMBIENT_STEP, out);
    return 1;
}

int ported_view_car_ratio(const Car *car, float out[3])
{
    return car != NULL && ratio_of_car(car, out);
}

int police_flash_on;   /* port (light_effects): the police car's lights flash (police_lights.h) */
void ported_view_set_police_lights(int on) { police_flash_on = on; }

/* the table for a car at `node`, NULL when its ambient is the world's */
const uint8_t *car_light(int32_t node, const double *xz, int car, const uint8_t palette[768])
{
    if (light_effects == NULL || palette == NULL || !(ambient_on || synth_on || (port_lamp_car_shade() && light_effects->car_pool != NULL) || light_effects->lamp_n > 0 || light_effects->car_tone != NULL)) return NULL;   /* port (lamp_car_shade): a made light with no tunnels has the lamps' pools to give its cars too */
    static struct { int key[8]; uint8_t table[256]; int used; } cache[AMBIENT_TABLES];   /* the ratio and the car's tone (track light file's car_tone) */
    static uint8_t made_for[768];
    static int next, valid;
    float ratio[3] = { 0.0f, 0.0f, 0.0f };   /* what a light with no ratio for the car left in it (found 0 left it unset: read as 0, so the car was black), now said */
    ratio_at_xz(node, xz, car, ratio);
    int key[8] = { 0 }, neutral = 1;
    const int floor_level = light_effects->pool != NULL ? port_car_night_lift() : 0;   /* port (car_night_lift): at night (the headlight pool), a floor of light on the body */
    key[7] = floor_level, neutral = floor_level == 0;
    const int match_bright = (port_car_tunnel_match() >= 1 || port_car_night_lift() >= 1) && (ratio[0] > 1.0f || ratio[1] > 1.0f || ratio[2] > 1.0f);   /* port (car_tunnel_match): above full light the table has no family */
    for (int k = 0; k < 3; k++) {
        key[k] = (int)(ratio[k] * AMBIENT_STEP + 0.5f);
        neutral = neutral && key[k] == AMBIENT_STEP;
    }
    if (light_effects->car_tone != NULL && light_effects->nodes > 0) {   /* port (track light file, car_tone): the car's own tone at this node */
        const uint8_t *tone = light_effects->car_tone + 4 * (size_t)light_tint_ring_node(node, light_effects->nodes);
        if (tone[3] > 0) {
            for (int k = 0; k < 4; k++) key[3 + k] = tone[k];
            neutral = 0;
        }
    }
    if (neutral) return NULL;
    if (!valid || memcmp(made_for, palette, sizeof made_for) != 0) {   /* a fade changed the palette */
        memset(cache, 0, sizeof cache);
        memcpy(made_for, palette, sizeof made_for), valid = 1, next = 0;
    }
    for (int i = 0; i < AMBIENT_TABLES; i++)
        if (cache[i].used && memcmp(cache[i].key, key, sizeof key) == 0) return cache[i].table;
    const int slot = next;
    next = (next + 1) % AMBIENT_TABLES;
    memcpy(cache[slot].key, key, sizeof key), cache[slot].used = 1;
    static const uint8_t FLOOR[3][3] = { { 0, 0, 0 }, { 8, 9, 12 }, { 16, 18, 24 } };   /* a cool light, as the night's */
    light_tint_lit_table_floor(palette, ratio, match_bright, floor_level > 0 && floor_level < 3 ? FLOOR[floor_level] : NULL, cache[slot].table);
    if (key[6] > 0) {
        const uint8_t rgb[3] = { (uint8_t)key[3], (uint8_t)key[4], (uint8_t)key[5] };
        light_tint_car_tone_table(palette, cache[slot].table, rgb, key[6], cache[slot].table);   /* in place: each entry reads only its own */
    }
    return cache[slot].table;
}

/* the table `car` is drawn through: its ambient's (car_light), and for the police car of a chase on, with its lights going to pure red and blue
 * past the light (police_lights.h) */
const uint8_t *car_table(const Car *car, const uint8_t palette[768])
{
    const double xz[2] = { car->x / 65536.0, car->z / 65536.0 };
    const uint8_t *base = car_light(car->node, xz, car->index, palette);
    if (!police_flash_on || palette == NULL || !police_lights_on(shadow_race, car)) return base;
    static uint8_t table[256];
    police_lights_table(base, palette, shadow_race->tick, table);
    return table;
}

