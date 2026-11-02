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

int psx_light_make_rows(const LightPos *pts, uint32_t count, int ring, int time, uint8_t *data)
{
    (void)ring;
    if (pts == NULL || data == NULL || count < 2 || time < PSX_SYNTH_DAWN || time > PSX_SYNTH_NIGHT) return 0;
    memset(data, PSX_LIGHT_FULL, (size_t)count * PSX_LIGHT_POINTS);
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
