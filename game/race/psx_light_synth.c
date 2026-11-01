/* The PS1 light of a track the CD has none for (psx_light_synth.h). */
#include "game/race/psx_light_synth.h"

#include <stdlib.h>
#include <strings.h>

/* the values a tunnel row reads as (psx_light_at): the road's cap, the wall foot's cap and the dim wall a 0 beyond it is */
enum { ROAD_IN = PSX_LIGHT_TUNNEL_ROAD, FOOT_IN = PSX_LIGHT_WALL, WALL_IN = PSX_LIGHT_WALL };

void psx_light_synth_tunnels(const uint8_t *property, uint32_t count, int ring, uint8_t *flags)
{
    for (uint32_t i = 0; i < count; i++) flags[i] = 0;
    uint32_t first = 0;   /* where the scan starts: on a ring, a node that is not a tunnel's, so no run is cut in two */
    if (ring) {
        while (first < count && (property[first] & PSX_SYNTH_PROPERTY_TUNNEL)) first++;
        if (first == count) first = 0;
    }
    uint32_t run = 0;
    for (uint32_t k = 0; k <= count; k++) {
        const uint32_t i = (first + k) % count;
        const int in = k < count && (property[i] & PSX_SYNTH_PROPERTY_TUNNEL);
        if (in) {
            run++;
            continue;
        }
        if (run >= PSX_SYNTH_MIN_RUN)
            for (uint32_t j = 0; j < run; j++) flags[(first + k - run + j) % count] = 1;
        run = 0;
    }
}

/* the distance from node i to the nearest flagged node, up to `limit` (limit + 1 when none is that near) */
static uint32_t near_tunnel(const uint8_t *flags, uint32_t count, int ring, uint32_t i, uint32_t limit)
{
    for (uint32_t d = 0; d <= limit; d++) {
        if (ring) {
            if (flags[(i + d) % count] || flags[(i + count - d % count) % count]) return d;
        } else if ((i + d < count && flags[i + d]) || (i >= d && flags[i - d])) {
            return d;
        }
    }
    return limit + 1;
}

static uint8_t lerp255(int to, uint32_t step, uint32_t steps)   /* 255 at step 0, `to` at step `steps` */
{
    return (uint8_t)(255 + (to - 255) * (int)step / (int)steps);
}

void psx_light_synth_rows(const uint8_t *flags, uint32_t count, int ring, uint8_t *data)
{
    static const int ROAD[] = { 0, 1, 6 }, FOOT[] = { 2, 7 }, BEYOND[] = { 3, 4, 5, 8, 9, 10 };
    for (uint32_t n = 0; n < count; n++) {
        uint8_t *row = data + (size_t)n * PSX_LIGHT_POINTS;
        for (int p = 0; p < PSX_LIGHT_POINTS; p++) row[p] = PSX_LIGHT_FULL;
        const uint32_t d = near_tunnel(flags, count, ring, n, PSX_SYNTH_RAMP);
        if (d > PSX_SYNTH_RAMP) continue;
        const uint32_t step = PSX_SYNTH_RAMP + 1 - d;   /* 1 at the ramp's far end .. PSX_SYNTH_RAMP + 1 in the tunnel */
        const int inside = d == 0;
        for (size_t k = 0; k < sizeof ROAD / sizeof *ROAD; k++) row[ROAD[k]] = inside ? PSX_LIGHT_FULL : lerp255(ROAD_IN, step, PSX_SYNTH_RAMP + 1);
        for (size_t k = 0; k < sizeof FOOT / sizeof *FOOT; k++) row[FOOT[k]] = inside ? PSX_LIGHT_FULL : lerp255(FOOT_IN, step, PSX_SYNTH_RAMP + 1);
        for (size_t k = 0; k < sizeof BEYOND / sizeof *BEYOND; k++) row[BEYOND[k]] = inside ? 0 : lerp255(WALL_IN, step, PSX_SYNTH_RAMP + 1);
    }
}

int psx_light_synth(const Tri *tri, PsxLight *out)
{
    const uint32_t count = tri->node_count;
    if (count < 2) return 0;
    uint8_t *property = malloc(count), *flags = malloc(count), *data = malloc((size_t)count * PSX_LIGHT_POINTS);
    int any = 0;
    if (property != NULL && flags != NULL && data != NULL) {
        for (uint32_t i = 0; i < count; i++) {
            TriNode node;
            property[i] = tri_node(tri, i, &node) == TRI_OK ? node.property : 0;
        }
        psx_light_synth_tunnels(property, count, tri_is_circuit(tri), flags);
        for (uint32_t i = 0; i < count; i++) any |= flags[i];
        if (any) {
            psx_light_synth_rows(flags, count, tri_is_circuit(tri), data);
            out->data = data;
            out->nodes = count;
            out->scripted = 0;
            out->reversed = 0;
            data = NULL;
        }
    }
    free(property), free(flags), free(data);
    return any;
}

int psx_light_synth_track(const char *track)
{
    return track != NULL && (strcasecmp(track, "TR6") == 0 || strcasecmp(track, "TR7") == 0);
}

float psx_light_synth_car(const PsxLight *light, uint32_t node)
{
    if (light == NULL || light->data == NULL) return 1.0f;
    const int road = psx_light_byte(light, node, 0);   /* 255 open or in the tunnel (stored), the ramp's line between */
    const int tunnel = psx_light_tunnel(light, node);
    const float open = 1.0f, inside = PSX_SYNTH_CAR_PERCENT / 100.0f;
    if (tunnel) return inside;
    if (road >= PSX_LIGHT_FULL) return open;
    const float f = (float)(PSX_LIGHT_FULL - road) / (float)(PSX_LIGHT_FULL - ROAD_IN);   /* 0 open .. 1 at the tunnel's door */
    return open + (inside - open) * f;
}
