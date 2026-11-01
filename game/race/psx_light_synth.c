/* The PS1 light of a track the CD has none for (psx_light_synth.h). */
#include "game/race/psx_light_synth.h"

#include <stdlib.h>
#include <strings.h>

/* the road property of a covered road: the tunnel bit and not the one beside it (TR7 778..818, property 5, is an open truss bridge: the sky shows through) */
static int is_tunnel(uint8_t property) { return (property & (PSX_SYNTH_PROPERTY_TUNNEL | PSX_SYNTH_PROPERTY_OPEN)) == PSX_SYNTH_PROPERTY_TUNNEL; }

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
        const int in = k < count && is_tunnel(property[i]);
        if (in) {
            run++;
            continue;
        }
        if (run >= PSX_SYNTH_MIN_RUN)
            for (uint32_t j = 0; j < run; j++) flags[(first + k - run + j) % count] = 1;
        run = 0;
    }
}

/* the distance from node i forward (ahead along the nodes) to the nearest flagged one, and back to it, up to `limit` (limit + 1 when none is that near) */
static uint32_t distance(const uint8_t *flags, uint32_t count, int ring, uint32_t i, uint32_t limit, int forward)
{
    for (uint32_t d = 0; d <= limit; d++) {
        if (ring) {
            if (flags[forward ? (i + d) % count : (i + count - d % count) % count]) return d;
        } else if (forward ? (i + d < count && flags[i + d]) : (i >= d && flags[i - d])) {
            return d;
        }
    }
    return limit + 1;
}

/* the points beyond the wall foot at `d` nodes from the tunnel (d 1 is the node next to its first or last), the CD's CY1 values by day; 255 past the table */
static const uint8_t BEYOND_ENTRY[PSX_SYNTH_BEYOND_ENTRY + 1] = { 0, 0, 0, 11, 99, 150, 192, 225 };
static const uint8_t BEYOND_EXIT[PSX_SYNTH_BEYOND_EXIT + 1] = { 0, 0, 0, 12, 30, 125, 162, 190, 206, 230, 255 };

/* a smooth S, 0 at step 0 and 1 at step `steps` (smoothstep) */
static float smooth(uint32_t step, uint32_t steps)
{
    const float t = (float)step / (float)steps;
    return t * t * (3.0f - 2.0f * t);
}

/* `inside` where the tunnel is, 255 beyond PSX_SYNTH_FOOT_RAMP nodes of it, an S between */
static uint8_t door_value(int inside, uint32_t d)
{
    if (d >= PSX_SYNTH_FOOT_RAMP) return PSX_LIGHT_FULL;
    return (uint8_t)(inside + (PSX_LIGHT_FULL - inside) * smooth(d, PSX_SYNTH_FOOT_RAMP) + 0.5f);
}

void psx_light_synth_rows(const uint8_t *flags, uint32_t count, int ring, int time, uint8_t *data)
{
    /* CY1 inside, by the time of the CD's files A, B, C: the road and the wall foot */
    static const int ROAD_INSIDE[3] = { 215, 255, 214 }, FOOT_INSIDE[3] = { 196, 130, 106 };
    const int road_inside = ROAD_INSIDE[time >= 0 && time <= 2 ? time : PSX_SYNTH_DAY], foot_inside = FOOT_INSIDE[time >= 0 && time <= 2 ? time : PSX_SYNTH_DAY];
    static const int ROAD[] = { 0, 1, 6 }, FOOT[] = { 2, 7 }, BEYOND[] = { 3, 4, 5, 8, 9, 10 };
    for (uint32_t n = 0; n < count; n++) {
        uint8_t *row = data + (size_t)n * PSX_LIGHT_POINTS;
        for (int p = 0; p < PSX_LIGHT_POINTS; p++) row[p] = PSX_LIGHT_FULL;
        const uint32_t ahead = distance(flags, count, ring, n, PSX_SYNTH_BEYOND_EXIT, 1), behind = distance(flags, count, ring, n, PSX_SYNTH_BEYOND_EXIT, 0);
        const uint32_t near = ahead < behind ? ahead : behind;   /* nodes to the tunnel, either way (0 inside) */
        int beyond = PSX_LIGHT_FULL, road = PSX_LIGHT_FULL, foot = PSX_LIGHT_FULL;
        if (near == 0) {
            beyond = 0, road = road_inside, foot = foot_inside;
        } else {
            if (ahead <= PSX_SYNTH_BEYOND_ENTRY && BEYOND_ENTRY[ahead] < beyond) beyond = BEYOND_ENTRY[ahead];   /* the entry: the tunnel is ahead */
            if (behind <= PSX_SYNTH_BEYOND_EXIT && BEYOND_EXIT[behind] < beyond) beyond = BEYOND_EXIT[behind];   /* the exit: it is behind */
            const int r = door_value(road_inside, near), f = door_value(foot_inside, near);
            road = r, foot = f;
        }
        for (size_t k = 0; k < sizeof ROAD / sizeof *ROAD; k++) row[ROAD[k]] = (uint8_t)road;
        for (size_t k = 0; k < sizeof FOOT / sizeof *FOOT; k++) row[FOOT[k]] = (uint8_t)foot;
        for (size_t k = 0; k < sizeof BEYOND / sizeof *BEYOND; k++) row[BEYOND[k]] = (uint8_t)beyond;
    }
}

int psx_light_synth(const Tri *tri, int time, PsxLight *out)
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
            psx_light_synth_rows(flags, count, tri_is_circuit(tri), time, data);
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
    const float dark = 1.0f - (float)psx_light_byte(light, node, 4) / (float)PSX_LIGHT_FULL;   /* how far the points beyond the foot have gone: 0 open, 1 in the tunnel */
    return 1.0f - (1.0f - PSX_SYNTH_CAR_PERCENT / 100.0f) * dark;
}
