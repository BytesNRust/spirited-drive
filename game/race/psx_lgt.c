#include "game/race/psx_lgt.h"

#include <stdlib.h>
#include <string.h>

enum { HEADER = 8 };

static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

int psx_lgt_parse(const uint8_t *file, size_t size, uint32_t expected_nodes, TrackNodeLight *out)
{
    if (file == NULL || size < HEADER || expected_nodes < 2) return 0;
    const uint32_t nodes = le32(file);
    if (nodes != expected_nodes || (size - HEADER) / TRACK_NODE_LIGHT_POINTS < nodes) return 0;
    uint8_t *data = malloc((size_t)nodes * TRACK_NODE_LIGHT_POINTS);
    if (data == NULL) return 0;
    memcpy(data, file + HEADER, (size_t)nodes * TRACK_NODE_LIGHT_POINTS);
    out->data = data;
    out->nodes = nodes;
    return 1;
}
