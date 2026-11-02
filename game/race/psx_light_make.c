#include "game/race/psx_light_make.h"

#include "game/race/track_geometry.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

uint8_t psx_light_level(float v)
{
    if (!(v > 0.0f)) return 0;
    if (v >= 1.0f) return PSX_LIGHT_FULL;
    return (uint8_t)(lroundf(v * 15.0f) * 17);
}

enum {
    SKY_SECTORS = 16,       /* azimuths round a point: the highest terrain in each is its horizon */
    SKY_WINDOW = 20,        /* nodes either side whose points can shade it */
    SKY_REACH = 80          /* units: what is further does not shade (the hills are the sky's) */
};

static int is_road_point(int p) { return p == 0 || p == 1 || p == 6; }

/* The sky a point sees: for each of SKY_SECTORS azimuths the highest elevation of the terrain points round it, and over that horizon the cosine-weighted share
 * of the dome left, 1 / (1 + tan^2) (a flat horizon sees all of it, a wall straight up none), averaged over the sectors. */
static float sky_visibility(const LightPos *pts, uint32_t count, int ring, uint32_t node, int point)
{
    const LightPos *at = &pts[node * PSX_LIGHT_POINTS + (uint32_t)point];
    float tan_max[SKY_SECTORS] = { 0 };
    for (int d = -SKY_WINDOW; d <= SKY_WINDOW; d++) {
        int64_t other = (int64_t)node + d;
        if (ring) other = ((other % count) + count) % count;
        else if (other < 0 || other >= count) continue;
        const LightPos *row = &pts[(uint32_t)other * PSX_LIGHT_POINTS];
        for (int q = 0; q < PSX_LIGHT_POINTS; q++) {
            const float dx = row[q].x - at->x, dz = row[q].z - at->z, dy = row[q].y - at->y;
            const float run = sqrtf(dx * dx + dz * dz);
            if (run < 0.5f || run > SKY_REACH) continue;
            const float t = dy / run;
            const int sector = (int)((atan2f(dz, dx) + 3.14159265f) * (float)SKY_SECTORS / 6.28318531f) % SKY_SECTORS;
            if (t > tan_max[sector]) tan_max[sector] = t;
        }
    }
    float seen = 0;
    for (int i = 0; i < SKY_SECTORS; i++) seen += 1.0f / (1.0f + tan_max[i] * tan_max[i]);
    return seen / (float)SKY_SECTORS;
}

int psx_light_make_rows(const LightPos *pts, uint32_t count, int ring, int time, uint8_t *data)
{
    if (pts == NULL || data == NULL || count < 2 || time < PSX_SYNTH_DAWN || time > PSX_SYNTH_NIGHT) return 0;
    for (uint32_t n = 0; n < count; n++)
        for (int p = 0; p < PSX_LIGHT_POINTS; p++)
            data[n * PSX_LIGHT_POINTS + (uint32_t)p] = is_road_point(p) ? PSX_LIGHT_FULL : psx_light_level(sky_visibility(pts, count, ring, n, p));
    return 1;
}

int psx_light_make(const Tri *tri, int time, PsxLight *out)
{
    if (tri == NULL || out == NULL || tri->node_count < 2) return 0;
    const uint32_t count = tri->node_count, segments = count / TRI_NODES_PER_SEGMENT;
    LightPos *pts = malloc((size_t)count * PSX_LIGHT_POINTS * sizeof *pts);
    uint8_t *data = malloc((size_t)count * PSX_LIGHT_POINTS);
    int ok = pts != NULL && data != NULL;
    for (uint32_t s = 0; ok && s < segments; s++) {
        TrackSegment seg;
        if (track_decode_segment(tri, s, &seg) != TRI_OK) ok = 0;
        else
            for (int r = 0; r < TRACK_SEGMENT_ROWS; r++)
                for (int p = 0; p < PSX_LIGHT_POINTS; p++) {
                    const TrackPoint *t = &seg.rows[r][p];
                    pts[(s * TRACK_SEGMENT_ROWS + (uint32_t)r) * PSX_LIGHT_POINTS + (uint32_t)p] = (LightPos){ (float)t->x / 65536.0f, (float)t->y / 65536.0f, (float)t->z / 65536.0f };
                }
    }
    ok = ok && psx_light_make_rows(pts, count, tri_is_circuit(tri), time, data);
    if (ok) {
        memset(out, 0, sizeof *out);
        out->data = data;
        out->nodes = count;
        data = NULL;
    }
    free(pts), free(data);
    return ok;
}
