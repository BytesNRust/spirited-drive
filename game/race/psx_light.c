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
    light->nodes = 0;
}

uint8_t psx_light_at(const PsxLight *light, uint32_t node, int point)
{
    if (light == NULL || light->data == NULL || light->nodes < 2 || point < 0 || point >= PSX_LIGHT_POINTS) return PSX_LIGHT_FULL;
    node %= light->nodes;
    if (node == light->nodes - 1) node--;
    const uint8_t *row = light->data + (size_t)node * PSX_LIGHT_POINTS;
    /* a 0 beyond a lit wall foot (point 2 right, 7 left) is no data, not black: the PS1 shows a tunnel's walls and pillars at about half light */
    const int foot = point >= 3 && point <= 5 ? row[2] : point >= 8 ? row[7] : 0;
    if (row[point] == 0 && foot >= PSX_LIGHT_WALL_FOOT) return PSX_LIGHT_WALL;
    return row[point];
}

uint8_t psx_light_node(const PsxLight *light, uint32_t node)
{
    static const int TERRAIN[] = { 2, 3, 4, 5, 7, 8, 9, 10 };
    if (light == NULL || light->data == NULL) return PSX_LIGHT_FULL;
    unsigned sum = 0;
    for (size_t i = 0; i < sizeof TERRAIN / sizeof *TERRAIN; i++) sum += psx_light_at(light, node, TERRAIN[i]);
    return (uint8_t)((sum + sizeof TERRAIN / sizeof *TERRAIN / 2) / (sizeof TERRAIN / sizeof *TERRAIN));
}
