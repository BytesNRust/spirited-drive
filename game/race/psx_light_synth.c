/* The PS1 light of a track the CD has none for (psx_light_synth.h). */
#include "game/race/psx_light_synth.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* the road property of a covered road: the tunnel bit and not the one beside it (TR7 778..818, property 5, is an open truss bridge: the sky shows through) */
static int is_tunnel(uint8_t property) { return (property & (PSX_SYNTH_PROPERTY_TUNNEL | PSX_SYNTH_PROPERTY_OPEN)) == PSX_SYNTH_PROPERTY_TUNNEL; }

/* a hall: TR7's steelworks (the start straight, nodes 1011..1023 and 0..62, property 17): a roof over the road, lit by lamps */
static int is_hall(uint8_t property) { return (property & PSX_SYNTH_PROPERTY_HALL) != 0; }

/* `value` in `flags` for each node of a run of PSX_SYNTH_MIN_RUN or more nodes `in` holds for; `blocks` is the bits that start a run, so the scan of a ring
 * starts on a node outside any (no run is cut in two) */
static void mark_runs(const uint8_t *property, uint32_t count, int ring, int (*in)(uint8_t), uint8_t blocks, uint8_t value, uint8_t *flags)
{
    uint32_t first = 0;
    if (ring) {
        while (first < count && (property[first] & blocks)) first++;
        if (first == count) first = 0;
    }
    uint32_t run = 0;
    for (uint32_t k = 0; k <= count; k++) {
        const uint32_t i = (first + k) % count;
        if (k < count && in(property[i])) {
            run++;
            continue;
        }
        if (run >= PSX_SYNTH_MIN_RUN)
            for (uint32_t j = 0; j < run; j++) flags[(first + k - run + j) % count] = value;
        run = 0;
    }
}

static int is_rain_cover(uint8_t property)
{
    return (property & PSX_SYNTH_PROPERTY_TUNNEL) != 0 && ((property & PSX_SYNTH_PROPERTY_OPEN) == 0 || (property & 2) != 0);
}

void psx_light_synth_rain_cover(const uint8_t *property, uint32_t count, int ring, uint8_t *flags)
{
    for (uint32_t i = 0; i < count; i++) flags[i] = 0;
    mark_runs(property, count, ring, is_rain_cover, PSX_SYNTH_PROPERTY_TUNNEL, 1, flags);
}

/* CL2's galleries (nodes 1144..1175 and 1464..1575, property 7): the tunnel bit with the open bit and the one beside it, a roof over the road with the sea open to one side. The CD's light darkens them as tunnels */
static int is_gallery(uint8_t property) { return (property & 7) == 7; }

void psx_light_synth_tunnels_ex(const uint8_t *property, uint32_t count, int ring, int gallery, uint8_t *flags)
{
    for (uint32_t i = 0; i < count; i++) flags[i] = 0;
    mark_runs(property, count, ring, is_hall, PSX_SYNTH_PROPERTY_HALL, PSX_SYNTH_FLAG_HALL, flags);
    mark_runs(property, count, ring, is_tunnel, PSX_SYNTH_PROPERTY_TUNNEL, PSX_SYNTH_FLAG_TUNNEL, flags);
    if (gallery) mark_runs(property, count, ring, is_gallery, PSX_SYNTH_PROPERTY_TUNNEL, PSX_SYNTH_FLAG_TUNNEL, flags);
}

void psx_light_synth_tunnels(const uint8_t *property, uint32_t count, int ring, uint8_t *flags)
{
    psx_light_synth_tunnels_ex(property, count, ring, 0, flags);
}

int psx_light_synth_gallery_track(const char *track)
{
    return track != NULL && strcasecmp(track, "CL2") == 0;
}

/* the distance from node i forward (ahead along the nodes) to the nearest flagged one, and back to it, up to `limit` (limit + 1 when none is that near) */
static uint32_t distance(const uint8_t *flags, uint32_t count, int ring, uint32_t i, uint32_t limit, int forward)
{
    for (uint32_t d = 0; d <= limit; d++) {
        if (ring) {
            if (flags[forward ? (i + d) % count : (i + count - d % count) % count] == PSX_SYNTH_FLAG_TUNNEL) return d;
        } else if (forward ? (i + d < count && flags[i + d] == PSX_SYNTH_FLAG_TUNNEL) : (i >= d && flags[i - d] == PSX_SYNTH_FLAG_TUNNEL)) {
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
    static const int ROAD_INSIDE[4] = { 215, 255, 214, 255 }, FOOT_INSIDE[4] = { 196, 130, 106, 255 };   /* the night's: the tint darkens it */
    const int road_inside = ROAD_INSIDE[time >= 0 && time <= 3 ? time : PSX_SYNTH_DAY], foot_inside = FOOT_INSIDE[time >= 0 && time <= 3 ? time : PSX_SYNTH_DAY];
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
        if (flags[n] == PSX_SYNTH_FLAG_HALL) row[PSX_SYNTH_HALL_POINT] = PSX_SYNTH_HALL_MARK;   /* the world's light unchanged (254 of 255), the car's alternation marked */
    }
}

int psx_light_synth_ex(const Tri *tri, int time, int gallery, PsxLight *out)
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
        psx_light_synth_tunnels_ex(property, count, tri_is_circuit(tri), gallery, flags);
        for (uint32_t i = 0; i < count; i++) any |= flags[i] != 0;
        if (any) {
            psx_light_synth_rows(flags, count, tri_is_circuit(tri), time, data);
            out->data = data;
            out->nodes = count;
            out->scripted = 0;
            out->reversed = 0;
            out->tunnel = flags;   /* the road's tunnel nodes go with it (made_tunnels) */
            flags = NULL;
            data = NULL;
        }
    }
    free(property), free(flags), free(data);
    return any;
}

int psx_light_synth(const Tri *tri, int time, PsxLight *out)
{
    return psx_light_synth_ex(tri, time, 0, out);
}

int psx_light_synth_trans(const char *track, uint32_t *open_from, uint32_t *open_to)
{
    if (track == NULL || strcasecmp(track, "TR7") != 0) return 0;
    if (open_from != NULL) *open_from = 700;
    if (open_to != NULL) *open_to = 776;
    return 1;
}

int psx_light_synth_trans_closed(const char *track, uint32_t *from, uint32_t *to, uint32_t *row_node)
{
    if (track == NULL || strcasecmp(track, "TR7") != 0) return 0;
    if (from != NULL) *from = 465;
    if (to != NULL) *to = 470;
    if (row_node != NULL) *row_node = 475;
    return 1;
}

int psx_light_synth_parking(const char *track, uint32_t *from, uint32_t *to)
{
    uint32_t closed_from = 0, hall_to = 0;
    if (!psx_light_synth_trans_closed(track, &closed_from, NULL, NULL) || !psx_light_synth_hall(track, NULL, &hall_to)) return 0;
    if (from != NULL) *from = closed_from;
    if (to != NULL) *to = hall_to;
    return 1;
}

int psx_light_synth_mine(const char *track)
{
    return track != NULL && strcasecmp(track, "TR6") == 0;
}

int psx_light_synth_cavern(const char *track, uint32_t *from, uint32_t *to)
{
    if (!psx_light_synth_mine(track)) return 0;
    if (from != NULL) *from = 322;
    if (to != NULL) *to = 546;
    return 1;
}

int psx_light_synth_track(const char *track)
{
    return track != NULL && (strcasecmp(track, "TR6") == 0 || strcasecmp(track, "TR7") == 0);
}

int psx_light_synth_hall(const char *track, uint32_t *from, uint32_t *to)
{
    if (track == NULL || strcasecmp(track, "TR7") != 0) return 0;
    if (from != NULL) *from = 471;
    if (to != NULL) *to = 570;
    return 1;
}

/* CY1's lamp context through the exe's table (psx_lgs.h): 128,115,96 under a lamp (even nodes), 60,60,60 between (odd) */
static const float LAMP_LIT[3] = { 96.0f / 160, 89.0f / 160, 80.0f / 160 };
static const float LAMP_DIM[3] = { 62.0f / 160, 62.0f / 160, 62.0f / 160 };
/* the steelworks hall's, warm orange like TR3's tunnel contexts (150,96,32 over the full 160): lit under a lamp, dimmer between */
static const float HALL_LIT[3] = { 150.0f / 160, 96.0f / 160, 32.0f / 160 };
static const float HALL_DIM[3] = { 96.0f / 160, 62.0f / 160, 21.0f / 160 };

void psx_light_synth_car(const PsxLight *light, uint32_t node, float out[3])
{
    const int beyond = light == NULL || light->data == NULL ? PSX_LIGHT_FULL : psx_light_byte(light, node, PSX_SYNTH_HALL_POINT);
    /* a hall: the made light's flag (the terrain's light, lower than the mark, replaces the mark in the row's byte), or the mark itself in a light that is the synth's alone */
    const int hall = light != NULL && ((light->hall != NULL && node < light->nodes && light->hall[node] != 0) || beyond == PSX_SYNTH_HALL_MARK);
    const float dark = hall ? 1.0f : 1.0f - (float)beyond / (float)PSX_LIGHT_FULL;   /* how far the points beyond the foot have gone: 0 open, 1 in the tunnel */
    const float *lamp = (node & 1u) != 0 ? (hall ? HALL_DIM : LAMP_DIM) : (hall ? HALL_LIT : LAMP_LIT);
    for (int k = 0; k < 3; k++) out[k] = 1.0f - (1.0f - lamp[k]) * dark;
}

void psx_light_synth_car_tunnel(const PsxLight *light, uint32_t node, float out[3])
{
    if (light != NULL && light->car_dim != NULL && light->nodes > 0 && light->car_dim[node % light->nodes]) {   /* port (parking_dark): the darkest tone of the lamp alternation (the exit tunnel's dim, which is right), whichever node */
        for (int k = 0; k < 3; k++) out[k] = LAMP_DIM[k];
        return;
    }
    if (light == NULL || light->tunnel == NULL || light->nodes == 0) {
        psx_light_synth_car(light, node, out);
        return;
    }
    const int hall = light->hall != NULL && node < light->nodes && light->hall[node] != 0;
    float dark = hall ? 1.0f : 0.0f;
    for (uint32_t d = 0; !hall && d < PSX_SYNTH_CAR_RAMP; d++) {   /* the nearest flagged node, either way along the ring */
        if (light->tunnel[(node + d) % light->nodes] || light->tunnel[(node + light->nodes - d % light->nodes) % light->nodes]) {
            dark = 1.0f - (float)d / (float)PSX_SYNTH_CAR_RAMP;
            break;
        }
    }
    const float *lamp = (node & 1u) != 0 ? (hall ? HALL_DIM : LAMP_DIM) : (hall ? HALL_LIT : LAMP_LIT);
    for (int k = 0; k < 3; k++) out[k] = 1.0f - (1.0f - lamp[k]) * dark;
}

void psx_light_synth_made_tunnels(PsxLight *made, PsxLight *tunnels, uint32_t hall_from, uint32_t hall_to)
{
    if (made == NULL || tunnels == NULL || tunnels->tunnel == NULL || tunnels->data == NULL || tunnels->nodes != made->nodes) return;
    for (uint32_t n = 0; n < made->nodes; n++) {
        const int hall = (made->hall != NULL && made->hall[n] != 0) || (n >= hall_from && n <= hall_to);
        if (hall) tunnels->tunnel[n] = 0;
        else if (tunnels->tunnel[n]) memcpy(made->data + (size_t)n * PSX_LIGHT_POINTS, tunnels->data + (size_t)n * PSX_LIGHT_POINTS, PSX_LIGHT_POINTS);
    }
    made->tunnel = tunnels->tunnel;
    tunnels->tunnel = NULL;
}
