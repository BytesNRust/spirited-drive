#include "game/race/psx_light_make.h"

#include "game/race/track_geometry.h"

#include <math.h>
#include <ctype.h>
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
static const float NIGHT_AMBIENT = 0.25f;   /* the terrain's light at night under an open sky */
static const float LAMP_POOL = 0.6f;        /* a lamp's added light under it, fading to the edge of its reach */

/* The day's light, dawn, day and dusk, fitted to the PS1 CD's (tools/psx_light_fit.c, 13 tracks, per terrain point) as light = c + k_sky * sky + k_nx * nx + k_ny * ny
 * with the surface's upward normal (nx, ny) and the share of the dome the point sees: the CD's is plain Lambert shading from a sun fixed along the track's x axis (at -x at
 * dawn, +x at dusk, high, near overhead by day), with no cast shadows and no darkening by distance or height. The outer points 5 and 10 follow nothing the terrain gives
 * (they fit to R2 under .3): they are their own mean. Rows are {c, k_sky, k_nx, k_ny} by point (0, 1 and 6 are the road's, never darkened). */
static const float FIT[3][PSX_LIGHT_POINTS][4] = {
    { { 0 }, { 0 }, { .288f, .025f, -.262f, .495f }, { .081f, .116f, -.293f, .580f }, { .260f, -.092f, -.278f, .601f }, { .650f, 0, 0, 0 }, { 0 },
      { .034f, .129f, -.332f, .677f }, { -.249f, .389f, -.281f, .706f }, { -.033f, .137f, -.306f, .739f }, { .591f, 0, 0, 0 } },   /* dawn */
    { { 0 }, { 0 }, { .171f, .165f, -.018f, .633f }, { -.309f, .398f, -.032f, .866f }, { .058f, -.031f, .008f, .926f }, { .638f, 0, 0, 0 }, { 0 },
      { .061f, .138f, .015f, .769f }, { -.254f, .360f, -.001f, .859f }, { .123f, -.065f, -.034f, .893f }, { .651f, 0, 0, 0 } },    /* day */
    { { 0 }, { 0 }, { .176f, .284f, .265f, .375f }, { -.302f, .404f, .267f, .731f }, { .090f, .036f, .295f, .701f }, { .589f, 0, 0, 0 }, { 0 },
      { .152f, .006f, .329f, .648f }, { .061f, .212f, .262f, .518f }, { .322f, -.085f, .236f, .534f }, { .654f, 0, 0, 0 } }      /* dusk */
};

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

void psx_light_features(const LightPos *pts, uint32_t count, int ring, uint32_t node, int point, PsxLightFeatures *out)
{
    PsxLightSun sun;
    psx_light_sun_default(PSX_SYNTH_DAY, &sun);
    const Horizon h = horizon_of(pts, count, ring, node, point, &sun);
    const LightPos n = normal_of(pts, count, ring, node, point);
    const LightPos *at = &pts[node * PSX_LIGHT_POINTS + (uint32_t)point], *centre = &pts[node * PSX_LIGHT_POINTS];
    const float dx = at->x - centre->x, dz = at->z - centre->z;
    *out = (PsxLightFeatures){ h.sky, h.sun_tan, n.x, n.y, n.z, at->y - centre->y, sqrtf(dx * dx + dz * dz) };
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

void psx_light_add_lamps(uint8_t *data, uint32_t count, int ring, const uint32_t *lamp_nodes, int n)
{
    if (data == NULL || lamp_nodes == NULL || count < 2) return;
    for (int i = 0; i < n; i++) {
        if (!ring && lamp_nodes[i] >= count) continue;
        const int64_t lamp = (int64_t)(lamp_nodes[i] % count);
        for (int d = -PSX_LIGHT_LAMP_REACH; d <= PSX_LIGHT_LAMP_REACH; d++) {
            int64_t node = lamp + d;
            if (ring) node = ((node % count) + count) % count;
            else if (node < 0 || node >= count) continue;
            const float r = (float)d / (float)(PSX_LIGHT_LAMP_REACH + 1), add = LAMP_POOL * (1.0f - r * r);
            for (int p = 0; p < PSX_LIGHT_POINTS; p++) {
                if (is_road_point(p)) continue;
                uint8_t *at = &data[(uint32_t)node * PSX_LIGHT_POINTS + (uint32_t)p];
                *at = psx_light_level((float)*at / (float)PSX_LIGHT_FULL + add);
            }
        }
    }
}

int psx_light_make_rows_sun(const LightPos *pts, uint32_t count, int ring, int time, const PsxLightSun *sun, uint8_t *data)
{
    if (pts == NULL || data == NULL || count < 2 || time < PSX_SYNTH_DAWN || time > PSX_SYNTH_NIGHT) return 0;
    PsxLightSun own;
    if (sun == NULL) psx_light_sun_default(time, &own), sun = &own;
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
            if (time == PSX_SYNTH_NIGHT) {
                *out = NIGHT_AMBIENT * h.sky;   /* the moon is no sun: the sky's share only, a dark night */
                continue;
            }
            const LightPos nm = normal_of(pts, count, ring, n, p);
            const float *k = FIT[time][p];
            *out = k[0] + k[1] * h.sky + k[2] * nm.x + k[3] * nm.y;
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

int psx_light_rules_parse(const char *text, size_t size, PsxLightRule *out, int max)
{
    if (text == NULL || out == NULL || max <= 0) return 0;
    int n = 0;
    for (size_t at = 0; at < size && n < max;) {
        char line[128];
        size_t len = 0;
        while (at < size && text[at] != '\n') {
            if (len < sizeof line - 1 && text[at] != '#') line[len++] = text[at];
            else if (text[at] == '#') {   /* the rest of the line is the comment */
                while (at < size && text[at] != '\n') at++;
                break;
            }
            at++;
        }
        at++;
        line[len] = '\0';
        long v[5];
        char *p = line, *end;
        int got = 0;
        for (; got < 5; got++) {
            while (*p != '\0' && isspace((unsigned char)*p)) p++;
            if (*p == '\0') break;
            v[got] = strtol(p, &end, 10);
            if (end == p) break;
            p = end;
        }
        while (*p != '\0' && isspace((unsigned char)*p)) p++;
        if (got != 5 || *p != '\0') continue;
        if (v[0] < 0 || v[1] < v[0] || v[2] < 0 || v[3] < v[2] || v[3] >= PSX_LIGHT_POINTS || v[4] < 0 || v[4] > PSX_LIGHT_FULL) continue;
        out[n++] = (PsxLightRule){ (int)v[0], (int)v[1], (int)v[2], (int)v[3], (int)v[4] };
    }
    return n;
}

void psx_light_apply_rules(uint8_t *data, uint32_t count, const PsxLightRule *rules, int n)
{
    if (data == NULL || rules == NULL) return;
    for (int i = 0; i < n; i++) {
        const uint8_t level = psx_light_level((float)rules[i].value / (float)PSX_LIGHT_FULL);
        for (int node = rules[i].node_from; node <= rules[i].node_to && (uint32_t)node < count; node++)
            for (int p = rules[i].point_from; p <= rules[i].point_to; p++) data[(uint32_t)node * PSX_LIGHT_POINTS + (uint32_t)p] = level;
    }
}

int psx_light_tri_points(const Tri *tri, LightPos *out)
{
    if (tri == NULL || out == NULL || tri->node_count < 2) return 0;
    const uint32_t segments = tri->node_count / TRI_NODES_PER_SEGMENT;
    for (uint32_t s = 0; s < segments; s++) {
        TrackSegment seg;
        if (track_decode_segment(tri, s, &seg) != TRI_OK) return 0;
        for (int r = 0; r < TRACK_SEGMENT_ROWS; r++)
            for (int p = 0; p < PSX_LIGHT_POINTS; p++) {
                const TrackPoint *t = &seg.rows[r][p];
                out[(s * TRACK_SEGMENT_ROWS + (uint32_t)r) * PSX_LIGHT_POINTS + (uint32_t)p] = (LightPos){ (float)t->x / 65536.0f, (float)t->y / 65536.0f, (float)t->z / 65536.0f };
            }
    }
    return 1;
}

int psx_light_make_lit(const Tri *tri, int time, const uint32_t *lamp_nodes, int lamps, PsxLight *out)
{
    if (tri == NULL || out == NULL || tri->node_count < 2) return 0;
    const uint32_t count = tri->node_count;
    LightPos *pts = malloc((size_t)count * PSX_LIGHT_POINTS * sizeof *pts);
    uint8_t *data = malloc((size_t)count * PSX_LIGHT_POINTS);
    int ok = pts != NULL && data != NULL && psx_light_tri_points(tri, pts);
    ok = ok && psx_light_make_rows(pts, count, tri_is_circuit(tri), time, data);
    if (ok && time == PSX_SYNTH_NIGHT) psx_light_add_lamps(data, count, tri_is_circuit(tri), lamp_nodes, lamps);
    if (ok) {
        memset(out, 0, sizeof *out);
        out->data = data;
        out->nodes = count;
        data = NULL;
    }
    free(pts), free(data);
    return ok;
}

int psx_light_make(const Tri *tri, int time, PsxLight *out)
{
    return psx_light_make_lit(tri, time, NULL, 0, out);
}
