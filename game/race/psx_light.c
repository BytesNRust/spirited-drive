#include "game/race/psx_light.h"

#include <stdlib.h>
#include <string.h>

enum { HEADER = 8 };

static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

int psx_light_parse(const uint8_t *file, size_t size, uint32_t expected_nodes, PsxLight *out)
{
    if (file == NULL || size < HEADER || expected_nodes < 2) return 0;
    const uint32_t nodes = le32(file);
    if (nodes != expected_nodes || (size - HEADER) / PSX_LIGHT_POINTS < nodes) return 0;
    uint8_t *data = malloc((size_t)nodes * PSX_LIGHT_POINTS);
    if (data == NULL) return 0;
    memcpy(data, file + HEADER, (size_t)nodes * PSX_LIGHT_POINTS);
    out->data = data;
    out->nodes = nodes;
    return 1;
}

void psx_light_free(PsxLight *light)
{
    free(light->data);
    free(light->hall);
    free(light->tunnel);
    free(light->car_dim);
    light->car_dim = NULL;
    free(light->canyon);
    light->canyon = NULL;
    free(light->pool_class);
    free(light->car_pool);
    light->car_pool = NULL;
    light->pool_class = NULL;
    light->data = NULL;
    light->hall = NULL;
    light->tunnel = NULL;
    light->scripted = 0;
    light->pooled = 0;
    light->reversed = 0;
    light->nodes = 0;
}

int psx_light_set_script(PsxLight *light, const PsxLgs *script)
{
    if (light == NULL || light->data == NULL || script == NULL) return 0;
    light->script = *script;
    light->scripted = 1;
    return 1;
}

int psx_light_set_reversed(PsxLight *light, int reversed)
{
    if (light == NULL || light->data == NULL) return 0;
    light->reversed = reversed != 0;
    return 1;
}

uint32_t psx_light_old_node(const PsxLight *light, uint32_t node)
{
    if (light == NULL || light->nodes == 0) return node;
    node %= light->nodes;
    return light->reversed ? (light->nodes - node) % light->nodes : node;
}

/* a point of a row as the file has it: the chains swap on a reversed track */
static int old_point(const PsxLight *light, int point)
{
    return light->reversed && point > 0 ? (point + 5 - 1) % 10 + 1 : point;
}

uint8_t psx_light_byte(const PsxLight *light, uint32_t node, int point)
{
    if (light == NULL || light->data == NULL || light->nodes < 2 || point < 0 || point >= PSX_LIGHT_POINTS) return PSX_LIGHT_FULL;
    node = psx_light_old_node(light, node);
    point = old_point(light, point);
    if (node == light->nodes - 1) node--;
    return light->data[(size_t)node * PSX_LIGHT_POINTS + (size_t)point];
}

static int row_is_tunnel(const uint8_t *row)
{
    return (row[2] >= PSX_LIGHT_WALL_FOOT && row[4] == 0 && row[5] == 0) || (row[7] >= PSX_LIGHT_WALL_FOOT && row[9] == 0 && row[10] == 0);
}

/* the tunnel test on a node of the file's own direction */
static int tunnel_in_file(const PsxLight *light, uint32_t node)
{
    if (light == NULL || light->data == NULL || light->nodes < 2) return 0;
    node %= light->nodes;
    if (node == light->nodes - 1) node--;
    if (light->tunnel != NULL && light->tunnel[node]) return 1;   /* a made light's road tunnel (made_tunnels); the rest is the rows' */
    if (light->scripted) return psx_lgs_switched(&light->script, (int)(node >> 2));
    return row_is_tunnel(light->data + (size_t)node * PSX_LIGHT_POINTS);
}

int psx_light_tunnel(const PsxLight *light, uint32_t node)
{
    return tunnel_in_file(light, psx_light_old_node(light, node));
}

int psx_light_tunnel_class(const PsxLight *light, uint32_t node)
{
    const uint32_t file_node = light != NULL ? psx_light_old_node(light, node) : 0;
    const int forest = light != NULL && light->forest_road && file_node >= light->forest_from && file_node <= light->forest_to;   /* port (forest_neutral): its shade is not a tunnel's */
    if (!forest && psx_light_tunnel(light, node)) return 1;
    if (light == NULL || light->pool_class == NULL || light->nodes == 0) return 0;
    const uint8_t c = light->pool_class[node % light->nodes];
    return c > 0 ? 1 + c : 0;
}

static uint8_t light_at(const PsxLight *light, uint32_t node, int point)
{
    if (light == NULL || light->data == NULL || light->nodes < 2 || point < 0 || point >= PSX_LIGHT_POINTS) return PSX_LIGHT_FULL;
    node = psx_light_old_node(light, node);
    point = old_point(light, point);
    if (node == light->nodes - 1) node--;
    const uint8_t *row = light->data + (size_t)node * PSX_LIGHT_POINTS;
    if (light->scripted) {   /* the exe's light: the context's colour or the LGT through T, as the luminance of the three channels */
        float f[3];
        psx_lgs_light(&light->script, (int)node, row[point], f);
        const float y = 0.30f * f[0] + 0.59f * f[1] + 0.11f * f[2];
        return (uint8_t)(y >= 1.0f ? 255 : y <= 0.0f ? 0 : y * 255.0f + 0.5f);
    }
    const int tunnel = tunnel_in_file(light, node);
    /* in a tunnel a 0 beyond a lit wall foot (point 2 right, 7 left) is no data, not black: the PS1 shows its walls and pillars at about half light;
     * anywhere else (a cutting, a bridge's shade) a 0 is the black it says */
    const int foot = point >= 3 && point <= 5 ? row[2] : point >= 8 ? row[7] : 0;
    if (tunnel && row[point] == 0 && foot >= PSX_LIGHT_WALL_FOOT) return PSX_LIGHT_WALL;
    /* the road inside a tunnel (a lit foot with 0s from point 4 on, either side) is dark on the PS1 although its bytes say 255 */
    if (tunnel && (point == 0 || point == 1 || point == 6) && row[point] > PSX_LIGHT_TUNNEL_ROAD) return PSX_LIGHT_TUNNEL_ROAD;
    if (tunnel && (point == 2 || point == 7) && row[point] > PSX_LIGHT_WALL) return PSX_LIGHT_WALL;   /* the shoulder's far edge: half light too */
    return row[point];
}

static int made_tunnel(const PsxLight *light, uint32_t node)
{
    return light->tunnel != NULL && light->nodes > 0 && light->tunnel[node % light->nodes] != 0;
}

/* xz: the point's place on the ground in metres, or NULL when only the node is known (the node's centre is used with headlight_cone) */
static uint8_t light_at_dark(const PsxLight *light, uint32_t node, int point, const double *xz)
{
    const uint8_t v = light_at(light, node, point);
    if (light == NULL || light->pool == NULL) return v;
    const int lift = light->pool->cone && xz != NULL ? headlight_pool_lift_xz(light->pool, xz[0], xz[1]) : headlight_pool_beam(light->pool, node);   /* port (headlight_cone): the beam at the point itself */
    if (!psx_light_tunnel(light, node) && !headlight_pool_tunnel(light->pool, node)) {
        int gain = headlight_pool_gain_at_lift(HEADLIGHT_FLOOR, lift);   /* the pool is as wide as the road and its shoulders: the ground beyond takes less of it */
        int floor = HEADLIGHT_FLOOR;
        if (light->night_lift > 0) {   /* port (city_lift, vegas_lift): the dark of the open night is less deep, a lit pool keeps its full light */
            floor += (256 - floor) * light->night_lift / 256;
            if (gain < floor) gain = floor;
        }
        const int share = light->pool->cone && xz != NULL ? 256 : headlight_pool_width(point);   /* port (headlight_cone): the cone has its own sides, so the verges and what stands beyond take the beam by where they are */
        const uint8_t open = headlight_pool_apply(v, floor + (gain - floor) * share / 256);
        if (light->beam_tunnel > 0 && light->hall != NULL && light->nodes > 0 && light->hall[node % light->nodes]) {   /* port (tunnel_headlight): the lit hall's walls and pillars take the beam too */
            const int hall_lit = light->beam_tunnel * lift / 256;
            return hall_lit > open ? (uint8_t)hall_lit : open;
        }
        return open;
    }
    /* a tunnel: less dark than the open night (its own lamps), and the beam lights it to TUNNEL_BEAM where its own rows are darker (a tunnel with no lamps is black) */
    const uint8_t scaled = made_tunnel(light, node) ? v : headlight_pool_apply(v, headlight_pool_gain_at_lift(TUNNEL_FLOOR, lift));   /* a made light's tunnel (made_tunnels) keeps the dusk's light its rows carry: the night's floor is the open road's */
    const int lit = (light->beam_tunnel > 0 ? light->beam_tunnel : TUNNEL_BEAM) * lift / 256;
    return lit > scaled ? (uint8_t)lit : scaled;
}

int psx_light_open_gain(const PsxLight *light, int lift)
{
    if (light == NULL || light->pool == NULL) return 256;
    int gain = headlight_pool_gain_at_lift(HEADLIGHT_FLOOR, lift);
    if (light->night_lift > 0) {
        const int floor = HEADLIGHT_FLOOR + (256 - HEADLIGHT_FLOOR) * light->night_lift / 256;
        if (gain < floor) gain = floor;
    }
    return gain;
}

uint8_t psx_light_at_xz(const PsxLight *light, uint32_t node, int point, const double *xz)
{
    const uint8_t v = light_at_dark(light, node, point, xz);
    /* port (vegas_objects): the points beyond the road's shoulder (3, 4, 5, 8..10: the walls and the buildings) keep almost full light, the road's own (0, 1, 2, 6, 7) take the dark and the pools */
    const int beyond = point == 3 || point == 4 || point == 5 || point >= 8;
    return light != NULL && light->object_floor > v && beyond ? light->object_floor : v;
}

uint8_t psx_light_at(const PsxLight *light, uint32_t node, int point) { return psx_light_at_xz(light, node, point, NULL); }

uint8_t psx_light_node_xz(const PsxLight *light, uint32_t node, const double *xz)
{
    static const int TERRAIN[] = { 2, 3, 4, 5, 7, 8, 9, 10 };
    if (light == NULL || light->data == NULL) return PSX_LIGHT_FULL;
    unsigned sum = 0;
    for (size_t i = 0; i < sizeof TERRAIN / sizeof *TERRAIN; i++) sum += psx_light_at_xz(light, node, TERRAIN[i], xz);
    return (uint8_t)((sum + sizeof TERRAIN / sizeof *TERRAIN / 2) / (sizeof TERRAIN / sizeof *TERRAIN));
}

uint8_t psx_light_node(const PsxLight *light, uint32_t node) { return psx_light_node_xz(light, node, NULL); }

int psx_light_car_pool(const PsxLight *light, uint32_t node, float out[3])
{
    static const float LAMP_GAIN[3] = { 2.6f, 2.4f, 1.8f };   /* at the lamp: the car's ratio times this, warm (the world's tint is dark, a ratio of 1 is the night as it is) */
    enum { RATIO_MAX = 2 };
    if (light == NULL || light->car_pool == NULL || light->nodes == 0 || out == NULL) return 0;
    const uint8_t c = light->car_pool[node % light->nodes];
    if (c == 0) return 0;
    for (int k = 0; k < 3; k++) {
        const float lifted = out[k] * (1.0f + (LAMP_GAIN[k] - 1.0f) * (float)c / 4.0f);
        if (lifted > out[k]) out[k] = lifted < (float)RATIO_MAX ? lifted : (out[k] > (float)RATIO_MAX ? out[k] : (float)RATIO_MAX);
    }
    return 1;
}
