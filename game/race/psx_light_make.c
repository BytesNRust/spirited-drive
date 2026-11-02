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

static const float SUN_BEAM_COS = 0.98f;    /* the terrain within about 11 degrees of the sun's azimuth can shade it */
static const float SHADOW_SOFT = 4.0f;      /* how fast the shadow's edge goes from lit to shaded with the horizon's tangent */
static const float AMBIENT_DAY = 0.45f, AMBIENT_LOW = 0.55f;   /* the sky's share of the light: the sun's is the rest */

static int is_road_point(int p) { return p == 0 || p == 1 || p == 6; }


typedef struct {
    float sky;        /* the share of the dome seen, 0..1 */
    float sun_tan;    /* the tangent of the highest terrain in the sun's direction (0 when none rises) */
} Horizon;

/* What a point sees round it: for each of SKY_SECTORS azimuths the highest elevation of the terrain points near it, and over that horizon the cosine-weighted
 * share of the dome left, 1 / (1 + tan^2) (a flat horizon sees all of it, a wall straight up none), averaged over the sectors; and the horizon toward the sun. */
static Horizon horizon_of(const LightPos *pts, uint32_t count, int ring, uint32_t node, int point, const PsxLightSun *sun)
{
    const LightPos *at = &pts[node * PSX_LIGHT_POINTS + (uint32_t)point];
    float tan_max[SKY_SECTORS] = { 0 };
    Horizon h = { 0, 0 };
    const float sun_x = cosf(sun->azimuth), sun_z = sinf(sun->azimuth);
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
            if ((dx * sun_x + dz * sun_z) / run > SUN_BEAM_COS && t > h.sun_tan) h.sun_tan = t;
        }
    }
    for (int i = 0; i < SKY_SECTORS; i++) h.sky += 1.0f / (1.0f + tan_max[i] * tan_max[i]);
    h.sky /= (float)SKY_SECTORS;
    return h;
}

static LightPos sub(LightPos a, LightPos b) { return (LightPos){ a.x - b.x, a.y - b.y, a.z - b.z }; }

/* The surface's upward normal at a point: along the road (the node after minus the one before) crossed with across it (the chain's neighbours, the centre's
 * two chains' first points). Straight up when the points do not span a surface. */
static LightPos normal_of(const LightPos *pts, uint32_t count, int ring, uint32_t node, int point)
{
    const uint32_t before = node > 0 ? node - 1 : (ring ? count - 1 : 0), after = node + 1 < count ? node + 1 : (ring ? 0 : count - 1);
    const LightPos *row = &pts[node * PSX_LIGHT_POINTS];
    const LightPos along = sub(pts[after * PSX_LIGHT_POINTS + (uint32_t)point], pts[before * PSX_LIGHT_POINTS + (uint32_t)point]);
    LightPos across;
    if (point == 0) across = sub(row[1], row[6]);
    else {
        const int chain_start = point <= 5 ? 1 : 6, first = chain_start, last = chain_start + 4;
        const int prev = point == first ? 0 : point - 1, next = point == last ? point : point + 1;
        across = sub(row[next], row[prev]);
        if (point > 5) across = (LightPos){ -across.x, -across.y, -across.z };   /* the left chain runs the other way: the same side of the road as the right's */
    }
    LightPos n = { along.y * across.z - along.z * across.y, along.z * across.x - along.x * across.z, along.x * across.y - along.y * across.x };
    const float len = sqrtf(n.x * n.x + n.y * n.y + n.z * n.z);
    if (len < 1e-4f) return (LightPos){ 0, 1, 0 };
    if (n.y < 0) n = (LightPos){ -n.x, -n.y, -n.z };
    return (LightPos){ n.x / len, n.y / len, n.z / len };
}

void psx_light_sun_default(int time, PsxLightSun *out)
{
    if (out == NULL) return;
    out->azimuth = 0.0f;
    out->elevation = time == PSX_SYNTH_DAY ? 0.87f : time == PSX_SYNTH_DAWN ? 0.26f : 0.21f;   /* about 50, 15 and 12 degrees */
}

void psx_light_smooth(float *light, uint32_t count, int ring)
{
    if (light == NULL || count < 2) return;
    float *copy = malloc((size_t)count * PSX_LIGHT_POINTS * sizeof *copy);
    if (copy == NULL) return;
    memcpy(copy, light, (size_t)count * PSX_LIGHT_POINTS * sizeof *copy);
    for (uint32_t n = 0; n < count; n++) {   /* along the road */
        const uint32_t before = n > 0 ? n - 1 : (ring ? count - 1 : n), after = n + 1 < count ? n + 1 : (ring ? 0 : n);
        for (int p = 0; p < PSX_LIGHT_POINTS; p++)
            if (!is_road_point(p))
                light[n * PSX_LIGHT_POINTS + (uint32_t)p] = 0.25f * copy[before * PSX_LIGHT_POINTS + (uint32_t)p] + 0.5f * copy[n * PSX_LIGHT_POINTS + (uint32_t)p] +
                                                            0.25f * copy[after * PSX_LIGHT_POINTS + (uint32_t)p];
    }
    memcpy(copy, light, (size_t)count * PSX_LIGHT_POINTS * sizeof *copy);
    for (uint32_t n = 0; n < count; n++)     /* along each chain: points 2..5 and 7..10 */
        for (int first = 2; first <= 7; first += 5)
            for (int p = first; p < first + 4; p++) {
                const float *row = &copy[n * PSX_LIGHT_POINTS];
                const float inner = p > first ? row[p - 1] : row[p], outer = p < first + 3 ? row[p + 1] : row[p];
                light[n * PSX_LIGHT_POINTS + (uint32_t)p] = 0.25f * inner + 0.5f * row[p] + 0.25f * outer;
            }
    free(copy);
}

int psx_light_make_rows_sun(const LightPos *pts, uint32_t count, int ring, int time, const PsxLightSun *sun, uint8_t *data)
{
    if (pts == NULL || data == NULL || count < 2 || time < PSX_SYNTH_DAWN || time > PSX_SYNTH_NIGHT) return 0;
    PsxLightSun own;
    if (sun == NULL) psx_light_sun_default(time, &own), sun = &own;
    const float up = sinf(sun->elevation), tan_sun = tanf(sun->elevation);
    const float sx = cosf(sun->elevation) * cosf(sun->azimuth), sy = up, sz = cosf(sun->elevation) * sinf(sun->azimuth);
    const float amb = time == PSX_SYNTH_DAY ? AMBIENT_DAY : AMBIENT_LOW, beam = 1.0f - amb;
    float *light = malloc((size_t)count * PSX_LIGHT_POINTS * sizeof *light);
    if (light == NULL) return 0;
    for (uint32_t n = 0; n < count; n++)
        for (int p = 0; p < PSX_LIGHT_POINTS; p++) {
            float *out = &light[n * PSX_LIGHT_POINTS + (uint32_t)p];
            if (is_road_point(p)) {
                *out = 1.0f;
                continue;
            }
            const Horizon h = horizon_of(pts, count, ring, n, p, sun);
            const LightPos nm = normal_of(pts, count, ring, n, p);
            float facing = nm.x * sx + nm.y * sy + nm.z * sz;
            if (facing < 0) facing = 0;
            float shadow = 0.5f + (tan_sun - h.sun_tan) * SHADOW_SOFT;   /* the horizon under the sun: lit; over it: shaded; a soft edge between */
            shadow = shadow < 0 ? 0 : shadow > 1 ? 1 : shadow;
            *out = (amb * h.sky + beam * facing * shadow) / (amb + beam * up);   /* flat, open and unshaded: exactly 1 */
        }
    psx_light_smooth(light, count, ring);
    for (size_t i = 0; i < (size_t)count * PSX_LIGHT_POINTS; i++) data[i] = psx_light_level(light[i]);
    free(light);
    return 1;
}

int psx_light_make_rows(const LightPos *pts, uint32_t count, int ring, int time, uint8_t *data)
{
    return psx_light_make_rows_sun(pts, count, ring, time, NULL, data);
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
