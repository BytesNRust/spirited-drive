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
    light->data = NULL;
    light->scripted = 0;
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
    if (light->scripted) return psx_lgs_switched(&light->script, (int)(node >> 2));
    return row_is_tunnel(light->data + (size_t)node * PSX_LIGHT_POINTS);
}

int psx_light_tunnel(const PsxLight *light, uint32_t node)
{
    return tunnel_in_file(light, psx_light_old_node(light, node));
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

uint8_t psx_light_at(const PsxLight *light, uint32_t node, int point)
{
    const uint8_t v = light_at(light, node, point);
    if (light == NULL || light->pool == NULL) return v;
    if (!psx_light_tunnel(light, node) && !headlight_pool_tunnel(light->pool, node)) return headlight_pool_apply(v, headlight_pool_gain(light->pool, node));
    /* a tunnel: less dark than the open night (its own lamps), and the beam lights it to TUNNEL_BEAM where its own rows are darker (a tunnel with no lamps is black) */
    const uint8_t scaled = headlight_pool_apply(v, headlight_pool_gain_tunnel(light->pool, node));
    const int lit = TUNNEL_BEAM * headlight_pool_beam(light->pool, node) / 256;
    return lit > scaled ? (uint8_t)lit : scaled;
}

uint8_t psx_light_node(const PsxLight *light, uint32_t node)
{
    static const int TERRAIN[] = { 2, 3, 4, 5, 7, 8, 9, 10 };
    if (light == NULL || light->data == NULL) return PSX_LIGHT_FULL;
    unsigned sum = 0;
    for (size_t i = 0; i < sizeof TERRAIN / sizeof *TERRAIN; i++) sum += psx_light_at(light, node, TERRAIN[i]);
    return (uint8_t)((sum + sizeof TERRAIN / sizeof *TERRAIN / 2) / (sizeof TERRAIN / sizeof *TERRAIN));
}
