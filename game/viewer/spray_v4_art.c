#include "viewer/spray_v4_art.h"

#include <math.h>
#include <string.h>

enum { N = SPRAY_V4_ART_SIZE };

static uint8_t art[SPRAY_V4_ART_FAMILIES][SPRAY_V4_ART_VARIANTS][N * N];
static uint8_t glint[SPRAY_V4_ART_VARIANTS][N * N];
static int made;

static uint32_t mix(uint32_t h)
{
    h ^= h >> 15, h *= 0x2c1b3c6du, h ^= h >> 12, h *= 0x297a2d39u, h ^= h >> 15;
    return h;
}

static float lattice(uint32_t seed, int x, int y)
{
    return (float)(mix((uint32_t)x * 0x9e3779b1u ^ (uint32_t)y * 0x85ebca6bu ^ seed * 0xc2b2ae35u) & 0xffff) / 65535.0f;
}

static float noise(uint32_t seed, float x, float y)
{
    const int ix = (int)floorf(x), iy = (int)floorf(y);
    const float fx = x - (float)ix, fy = y - (float)iy, tx = fx * fx * (3.0f - 2.0f * fx), ty = fy * fy * (3.0f - 2.0f * fy);
    return (lattice(seed, ix, iy) * (1.0f - tx) + lattice(seed, ix + 1, iy) * tx) * (1.0f - ty) + (lattice(seed, ix, iy + 1) * (1.0f - tx) + lattice(seed, ix + 1, iy + 1) * tx) * ty;
}

static float clamp01(float v) { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }
static float smooth(float a, float b, float v) { const float t = clamp01((v - a) / (b - a)); return t * t * (3.0f - 2.0f * t); }
static float rnd(uint32_t *r) { *r = mix(*r * 1664525u + 1013904223u); return (float)(*r >> 8 & 0xffff) / 65535.0f; }

/* a pixel's place in -1..1 */
static float at(int i) { return ((float)i + 0.5f) / (float)(N / 2) - 1.0f; }

/* the field to bytes, the border cleared; `top` is what maps to 255 (the field's own peak when 0) */
static void store(uint8_t *out, const float *f, float top)
{
    float best = top;
    if (best <= 0.0f)
        for (int i = 0; i < N * N; i++) best = f[i] > best ? f[i] : best;
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            const float v = best > 0.0f ? f[y * N + x] / best : 0.0f;
            out[y * N + x] = (uint8_t)(x == 0 || y == 0 || x == N - 1 || y == N - 1 ? 0.0f : clamp01(v) * 255.0f + 0.5f);
        }
}

static float field[N * N];

/* ---- the water's spray cluster: a loose cluster of many tiny speckles (a dot each, one pixel at the sprite's scale), denser at the core, the edge ragged ---- */
static void drop_make(uint8_t *out, int variant)
{
    const uint32_t seed = 71u + (uint32_t)variant * 19u;
    static const float REACH[SPRAY_V4_ART_VARIANTS] = { 0.55f, 0.65f, 0.75f, 0.85f, 0.95f }, FILL[SPRAY_V4_ART_VARIANTS] = { 0.60f, 0.55f, 0.50f, 0.45f, 0.40f };
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            const float u = at(x), v = at(y), rr = sqrtf(u * u + v * v) / REACH[variant];
            const float ragged = rr + 0.25f * (noise(seed, u * 6.0f, v * 6.0f) - 0.5f) + 0.1f * (noise(seed + 1u, u * 15.0f, v * 15.0f) - 0.5f);
            const float density = powf(clamp01(1.0f - ragged), 1.4f) * FILL[variant];   /* the core densest */
            field[y * N + x] = lattice(seed + 2u, x, y) < density ? 0.55f + 0.45f * lattice(seed + 3u, x, y) : 0.0f;
        }
    store(out, field, 1.0f);
}

/* ---- the mist: a soft round puff, no streaks, no flat sheet (it is a billboard) ---- */
static void mist_make(uint8_t *out, int variant)
{
    const uint32_t seed = 101u + (uint32_t)variant * 13u;
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            const float u = at(x), v = at(y), rr = sqrtf(u * u + v * v);
            const float edge = rr >= 1.0f ? 0.0f : powf(1.0f - rr * rr, 1.6f);
            const float patch = 0.6f + 0.4f * noise(seed, u * 3.5f + 7.0f, v * 3.5f + 7.0f) + 0.15f * (noise(seed + 3u, u * 9.0f, v * 9.0f) - 0.5f);
            field[y * N + x] = edge * patch;
        }
    store(out, field, 0.0f);
}

/* ---- the gravel's stones: the snow's way (many scattered single-texel specks, denser in the middle, the sprite growing from variant A to E), in gravel's strengths: each speck darker or lighter, and a few with a glint ---- */
static void stone_make(uint8_t *out, uint8_t *glint_out, int variant)
{
    const uint32_t seed = 511u + (uint32_t)variant * 29u;
    static const float REACH[SPRAY_V4_ART_VARIANTS] = { 0.45f, 0.55f, 0.65f, 0.75f, 0.85f };
    memset(glint_out, 0, N * N);
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            const float u = at(x), v = at(y), rr = sqrtf(u * u + v * v) / REACH[variant];
            const float frayed = rr + 0.25f * (noise(seed, u * 7.0f, v * 7.0f) - 0.5f) + 0.1f * (noise(seed + 1u, u * 19.0f, v * 19.0f) - 0.5f);
            const float density = clamp01(1.0f - frayed) * 0.16f;   /* the middle a bit denser, with space between the specks */
            float val = 0.0f;
            if (lattice(seed + 3u, x, y) < density) {
                val = 0.55f + 0.45f * lattice(seed + 4u, x, y);   /* a darker or a lighter stone */
                if (val > 0.8f && lattice(seed + 6u, x, y) < 0.3f) glint_out[y * N + x] = 255;   /* a lit one */
            }
            field[y * N + x] = val;
        }
    store(out, field, 1.0f);
}

/* ---- the cotton cloud: round lobes with a soft edge, the core the densest ---- */
static void cloud_make(uint8_t *out, int variant, int lobes, float spread, float rmin, float rmax, float core, uint32_t seed, float grain)
{
    float cx[40], cy[40], cr[40];
    uint32_t r = seed * 2654435761u + (uint32_t)variant * 7919u + 1u;
    for (int i = 0; i < lobes; i++) {
        const float ang = rnd(&r) * 6.2831853f, dist = sqrtf(rnd(&r)) * spread;
        cx[i] = cosf(ang) * dist, cy[i] = sinf(ang) * dist * 0.9f, cr[i] = rmin + (rmax - rmin) * rnd(&r);
        cr[i] *= 1.0f - 0.45f * dist / spread;   /* the lobes at the rim are the smaller */
    }
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            const float u = at(x), v = at(y);
            float sum = 0.0f, top = 0.0f;
            for (int i = 0; i < lobes; i++) {
                const float d = sqrtf((u - cx[i]) * (u - cx[i]) + (v - cy[i]) * (v - cy[i]));
                const float l = 1.0f - smooth(cr[i] * 0.15f, cr[i], d);   /* a wide soft edge: the lobes run into one another */
                sum += l, top = l > top ? l : top;
            }
            const float rr = sqrtf(u * u + v * v);
            float val = 0.45f * top + 0.55f * (1.0f - expf(-sum * 1.1f)) + core * (1.0f - smooth(0.0f, 0.6f, rr));   /* a mass that builds up where the lobes overlap, densest at the core */
            val *= 1.0f - smooth(0.78f, 1.0f, rr);
            val *= 1.0f - grain + grain * (0.5f + noise(seed + 5u, u * 9.0f, v * 9.0f));
            field[y * N + x] = val;
        }
    store(out, field, 1.0f + core);
}

/* ---- the snow's powder: fine mottled grain, loose bright sparks, frayed edges ---- */
static void snow_make(uint8_t *out, int variant)
{
    const uint32_t seed = 211u + (uint32_t)variant * 29u;
    static const float REACH[SPRAY_V4_ART_VARIANTS] = { 0.55f, 0.65f, 0.75f, 0.85f, 0.95f };
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            const float u = at(x), v = at(y), rr = sqrtf(u * u + v * v) / REACH[variant];
            const float frayed = rr + 0.28f * (noise(seed, u * 7.0f, v * 7.0f) - 0.5f) + 0.12f * (noise(seed + 1u, u * 19.0f, v * 19.0f) - 0.5f);
            const float density = clamp01(1.0f - frayed) * (0.4f + 0.9f * noise(seed + 2u, u * 4.5f + 3.0f, v * 4.5f + 3.0f));   /* mottled */
            const float g = lattice(seed + 3u, x, y);   /* the grain: each pixel on or off */
            float val = g < density * 1.1f ? 0.45f + 0.55f * lattice(seed + 4u, x, y) : 0.0f;
            if (val == 0.0f && rr < 1.35f && lattice(seed + 5u, x, y) > 0.985f - 0.01f * (float)variant) val = 1.0f;   /* a loose bright spark */
            field[y * N + x] = val;
        }
    store(out, field, 1.0f);
}

/* ---- a lump of snow / a stone: an irregular body, its radius jittered round it ---- */
static void lump_make(uint8_t *out, uint8_t *glint_out, int variant, int vertices, float size, int angular, uint32_t seed)
{
    float rad[12];
    uint32_t r = seed * 2654435761u + (uint32_t)variant * 104729u + 1u;
    for (int i = 0; i < vertices; i++) rad[i] = size * (angular ? 0.45f + 0.75f * rnd(&r) : 0.7f + 0.45f * rnd(&r));
    const float tilt = rnd(&r) * 6.2831853f;
    float gx = 0.0f, gy = 0.0f;   /* the glint: at the vertex facing up-left */
    float best = -9.0f;
    for (int i = 0; i < vertices; i++) {
        const float a = tilt + 6.2831853f * (float)i / (float)vertices, px = cosf(a) * rad[i], py = sinf(a) * rad[i], facing = -px - py;
        if (facing > best) best = facing, gx = px * 0.55f, gy = py * 0.55f;
    }
    if (angular) {   /* a real polygon: inside by its edges, the rim one pixel wide, the corners sharp */
        float vx[12], vy[12];
        for (int i = 0; i < vertices; i++) vx[i] = cosf(tilt + 6.2831853f * (float)i / (float)vertices) * rad[i], vy[i] = sinf(tilt + 6.2831853f * (float)i / (float)vertices) * rad[i];
        for (int y = 0; y < N; y++)
            for (int x = 0; x < N; x++) {
                const float u = at(x), v = at(y);
                float inside = -9.0f;   /* the signed distance to the nearest edge: positive inside (a convex polygon) */
                for (int i = 0; i < vertices; i++) {
                    const int j = (i + 1) % vertices;
                    const float ex = vx[j] - vx[i], ey = vy[j] - vy[i], len = sqrtf(ex * ex + ey * ey), d = (ex * (v - vy[i]) - ey * (u - vx[i])) / (len > 0.0f ? len : 1.0f);
                    if (inside < -8.0f || d < inside) inside = d;
                }
                field[y * N + x] = clamp01(inside * (float)(N / 2) + 0.5f);
            }
    } else
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            const float u = at(x), v = at(y), rr = sqrtf(u * u + v * v);
            float a = atan2f(v, u) - tilt;
            while (a < 0.0f) a += 6.2831853f;
            const float pos = a / 6.2831853f * (float)vertices;
            const int i0 = (int)pos % vertices, i1 = (i0 + 1) % vertices;
            const float t = pos - floorf(pos), edge = angular ? rad[i0] * (1.0f - t) + rad[i1] * t : rad[i0] * (1.0f - 0.5f * (1.0f - cosf(t * 3.1415927f))) + rad[i1] * 0.5f * (1.0f - cosf(t * 3.1415927f));
            field[y * N + x] = angular ? 1.0f - smooth(edge - 0.025f, edge + 0.01f, rr) : (1.0f - smooth(edge - 0.07f, edge + 0.01f, rr)) * (0.75f + 0.25f * noise(seed, u * 6.0f, v * 6.0f));
        }
    store(out, field, 1.0f);
    if (glint_out != NULL) {
        memset(glint_out, 0, N * N);
        const int cx = (int)((gx + 1.0f) * (float)(N / 2)), cy = (int)((gy + 1.0f) * (float)(N / 2));
        for (int dy = -2; dy <= 1; dy++)
            for (int dx = -2; dx <= 1; dx++) {
                const int px = cx + dx, py = cy + dy;
                if (px > 0 && py > 0 && px < N - 1 && py < N - 1 && out[py * N + px] > 200) glint_out[py * N + px] = 255;
            }
    }
}

static void make(void)
{
    if (made) return;
    made = 1;
    for (int v = 0; v < SPRAY_V4_ART_VARIANTS; v++) {
        drop_make(art[SPRAY_V4_ART_DROP][v], v);
        mist_make(art[SPRAY_V4_ART_MIST][v], v);
        cloud_make(art[SPRAY_V4_ART_EARTH][v], v, 8 + v * 2, 0.36f + 0.05f * (float)v, 0.26f, 0.42f, 0.7f, 301u, 0.12f);
        snow_make(art[SPRAY_V4_ART_SNOW][v], v);
        lump_make(art[SPRAY_V4_ART_CLUMP][v], NULL, v, 9, 0.30f + 0.07f * (float)v, 0, 401u);
        stone_make(art[SPRAY_V4_ART_STONE][v], glint[v], v);
        cloud_make(art[SPRAY_V4_ART_FINE_DUST][v], v, 22 + v * 2, 0.55f + 0.05f * (float)v, 0.14f, 0.26f, 0.15f, 601u, 0.5f);
    }
}

const uint8_t *spray_v4_art(int family, int variant)
{
    make();
    return family < 0 || family >= SPRAY_V4_ART_FAMILIES || variant < 0 || variant >= SPRAY_V4_ART_VARIANTS ? NULL : art[family][variant];
}

const uint8_t *spray_v4_art_glint(int family, int variant)
{
    make();
    return family != SPRAY_V4_ART_STONE || variant < 0 || variant >= SPRAY_V4_ART_VARIANTS ? NULL : glint[variant];
}
