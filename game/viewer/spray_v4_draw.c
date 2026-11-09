#include "viewer/spray_v4_draw.h"

#include "game/gfx/sprite_scaled.h"
#include "game/race/spray_tint.h"
#include "viewer/spray_v4_art.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* the drawn kinds: the particles' own (SprayV4Kind) and the stones' glint */
enum { HALF_RX = 48, WORLD_SHIFT = 11, MAX_CHUNKS = 4096, BAND = 8, MAX_PUFF_W = 4096, NODE_BUCKETS = 1024, LEVELS = 8, GLINT = SPRAY_V4_KINDS, KINDS = SPRAY_V4_KINDS + 1 };

/* how each kind is drawn: its sprite family, its colour's cap (a level k mixes k/per toward it), how far the sprite's half width is from the particle's radius (the body of the sprite is a fraction of it), its height over
 * its width (1: a billboard, never squashed), how far it sits above the road, and the distances it fades in (from `near0` to `near1` metres) and out (`far0` to `far1`) */
typedef struct { int family; int per; float scale, flat, lift, near0, near1, far0, far1; } Look;
static const Look LOOKS[KINDS] = {
    [SPRAY_V4_MIST]   = { SPRAY_V4_ART_MIST,      16, 1.7f, 1.00f, 0.25f, 2.0f, 4.5f, 80.0f, 300.0f },
    [SPRAY_V4_GRAVEL] = { SPRAY_V4_ART_STONE,     12, 1.5f, 1.00f, 0.00f, 1.5f, 3.5f, 30.0f, 90.0f },
    [SPRAY_V4_DUST]   = { SPRAY_V4_ART_FINE_DUST, 20, 1.5f, 1.00f, 0.30f, 2.0f, 4.5f, 80.0f, 300.0f },
    [SPRAY_V4_DROP]   = { SPRAY_V4_ART_DROP,      10, 1.4f, 1.00f, 0.00f, 1.5f, 3.5f, 40.0f, 120.0f },
    [SPRAY_V4_EARTH]  = { SPRAY_V4_ART_EARTH,      8, 2.0f, 1.00f, 0.30f, 2.0f, 4.5f, 80.0f, 300.0f },
    [SPRAY_V4_SNOW]   = { SPRAY_V4_ART_SNOW,      11, 1.5f, 1.00f, 0.30f, 2.0f, 4.5f, 80.0f, 300.0f },
    [SPRAY_V4_CLUMP]  = { SPRAY_V4_ART_CLUMP,     10, 2.2f, 1.00f, 0.00f, 1.5f, 3.5f, 40.0f, 120.0f },
    [GLINT]           = { SPRAY_V4_ART_STONE,      8, 1.5f, 1.00f, 0.00f, 1.5f, 3.5f, 30.0f, 90.0f }
};
static const float BODY_KEEP = 0.2f;   /* the share of a puff's alpha over the body of the car it came from */
static uint8_t LEVEL_AT[KINDS][4][4][256];   /* [kind][y & 3][x & 3][alpha]: the level a pixel of that alpha gets with nothing before it */
static int RCP[256], DITHER[16];   /* 65536 / (255 - before); the Bayer 4x4 thresholds in 16.16 */
static void fixed_tables(void)
{
    static const int bayer[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };
    for (int b = 0; b < 255; b++) RCP[b] = 65536 / (255 - b);
    RCP[255] = 0;
    for (int i = 0; i < 16; i++) DITHER[i] = (bayer[i] * 2 + 1) * 2048;
    for (int kind = 0; kind < KINDS; kind++)
        for (int q = 0; q < 16; q++)
            for (int a = 0; a < 256; a++) {
                const int k = (a * LOOKS[kind].per + (bayer[q] * 2 + 1) * 8) >> 8;
                LEVEL_AT[kind][q >> 2][q & 3][a] = (uint8_t)(k > LEVELS ? LEVELS : k);
            }
}

static SprayV4 state;
static SprayV4Host H;
static uint8_t identity[256];
static uint8_t tables[KINDS][LEVELS + 1][256];
static const uint8_t *rows[KINDS][256];
static uint8_t made_palette[768], made_rgb[KINDS][3];
static int tables_made;
static uint8_t *frame_map[KINDS], *group_map, *half_map;   /* half_map: the big puffs' alpha at half the resolution (a cell is 2x2 pixels of the map), the dither is still per pixel */
static size_t frame_cap, group_cap;
static int map_w, map_h;
static int *row_lo, *row_hi;   /* the extent of each row the group's puffs touch */
static int *half_lo, *half_hi;   /* the same for the half-resolution rows (in cells) */
static int half_w;
static size_t now_cap;
static uint8_t *fm_used, *now_row;   /* per kind and row: an earlier group put something in the frame map (else the pixel's `before` is 0); a row of the group's alpha */
static size_t row_cap;
static int16_t node_head[NODE_BUCKETS], node_next[SPRAY_V4_MAX];   /* the live particles by road node (chained in a hash of it), built each frame */
static int chunk_count;   /* the strips made this frame: kept until the next, the recorder replays them when the frame's 3D is whole */
enum { ARENA_BLOCKS = 16, ARENA_BLOCK = 4 << 20 };
static uint8_t *arena[ARENA_BLOCKS];
static size_t arena_cap[ARENA_BLOCKS], arena_used[ARENA_BLOCKS];

/* `n` zeroed bytes valid until the next frame (blocks are kept and reused: a strip is a few hundred KB, and allocating each from the system every frame costs more than filling it) */
static uint8_t *arena_zeroed(size_t n)
{
    for (int b = 0; b < ARENA_BLOCKS; b++) {
        if (arena[b] == NULL) {
            arena_cap[b] = n > ARENA_BLOCK ? n : ARENA_BLOCK, arena[b] = malloc(arena_cap[b]);
            if (arena[b] == NULL) return NULL;
        }
        if (arena_used[b] + n > arena_cap[b]) continue;
        uint8_t *at = arena[b] + arena_used[b];
        arena_used[b] += n;
        return memset(at, 0, n);
    }
    return NULL;
}

SprayV4 *spray_v4_state(void) { return &state; }

/* the sprite's column or row (0..SPRAY_V4_ART_SIZE - 1) at the unit-square coordinate u in -1..1 (nearest, on the texel centres: the dither hides the step), or -1 outside */
static int cell(float u, int flip)
{
    if (u <= -1.0f || u >= 1.0f) return -1;
    const int c = (int)(((flip ? -u : u) + 1.0f) * 0.5f * (float)(SPRAY_V4_ART_SIZE - 1) + 0.5f);   /* the texel centres on the same grid the bilinear sample had, so the map keeps its mean */
    return c < 0 ? 0 : c >= SPRAY_V4_ART_SIZE ? SPRAY_V4_ART_SIZE - 1 : c;
}

static void tables_set(const uint8_t rgb[SPRAY_V4_KINDS][3])
{
    uint8_t want[KINDS][3];
    memcpy(want, rgb, sizeof want[0] * SPRAY_V4_KINDS), memcpy(want[GLINT], rgb[SPRAY_V4_DUST], 3);
    if (tables_made && memcmp(made_palette, H.palette, sizeof made_palette) == 0 && memcmp(made_rgb, want, sizeof want) == 0) return;
    memcpy(made_palette, H.palette, sizeof made_palette), memcpy(made_rgb, want, sizeof want);
    for (int i = 0; i < 256; i++) identity[i] = (uint8_t)i;
    for (int kind = 0; kind < KINDS; kind++) {
        memcpy(tables[kind][0], identity, 256);
        for (int k = 1; k <= LEVELS; k++) spray_tint_table(H.palette, made_rgb[kind], k * 100 / LOOKS[kind].per, tables[kind][k]);
        for (int i = 0; i < 256; i++) rows[kind][i] = tables[kind][0];
        for (int k = 0; k <= LEVELS; k++) rows[kind][k] = tables[kind][k];
    }
    tables_made = 1;
}

void spray_v4_draw_frame(const SprayV4Host *host, const uint8_t rgb[SPRAY_V4_KINDS][3])
{
    H = *host;
    tables_set(rgb);
    for (int b = 0; b < ARENA_BLOCKS; b++) arena_used[b] = 0;
    chunk_count = 0;
    map_w = H.target->clip_right - H.target->clip_left, map_h = H.target->clip_bottom - H.target->clip_top;
    if (map_w <= 0 || map_h <= 0) { map_w = map_h = 0; return; }
    if ((size_t)map_h > row_cap || (size_t)map_w + 8 > now_cap) {
        now_cap = (size_t)map_w + 8;
        free(row_lo), free(row_hi), free(half_lo), free(half_hi), free(fm_used), free(now_row);
        fm_used = malloc((size_t)(KINDS + 2) * (size_t)map_h), now_row = malloc((size_t)map_w + 8);
        row_lo = malloc((size_t)map_h * sizeof *row_lo), row_hi = malloc((size_t)map_h * sizeof *row_hi);
        half_lo = malloc((size_t)(map_h / 2 + 1) * sizeof *half_lo), half_hi = malloc((size_t)(map_h / 2 + 1) * sizeof *half_hi);
        row_cap = row_lo != NULL && row_hi != NULL && half_lo != NULL && half_hi != NULL && fm_used != NULL && now_row != NULL ? (size_t)map_h : 0;
        if (row_cap == 0) { map_w = map_h = 0; return; }
    }
    for (int i = 0; i < NODE_BUCKETS; i++) node_head[i] = -1;
    for (int i = 0; i < SPRAY_V4_MAX; i++)
        if (state.p[i].live) {
            const uint32_t b = (uint32_t)state.p[i].node & (NODE_BUCKETS - 1);
            node_next[i] = node_head[b], node_head[b] = (int16_t)i;
        }
    half_w = map_w / 2 + 1;
    if (RCP[0] == 0) fixed_tables();
    const size_t need = (size_t)map_w * (size_t)map_h;
    if (need > frame_cap) {
        int ok = 1;
        for (int k = 0; k < KINDS; k++) { free(frame_map[k]); frame_map[k] = calloc(need, 1), ok &= frame_map[k] != NULL; }
        free(group_map), group_map = calloc(need, 1);
        free(half_map), half_map = calloc((size_t)(map_w / 2 + 1) * (size_t)(map_h / 2 + 1), 1);
        frame_cap = group_cap = need;
        if (!ok || group_map == NULL || half_map == NULL) { frame_cap = group_cap = 0, map_w = map_h = 0; return; }
    }
    for (int k = 0; k < KINDS; k++) memset(frame_map[k], 0, need);
    memset(fm_used, 0, (size_t)(KINDS + 2) * (size_t)map_h);
}

typedef struct { float cx, cy, rx, ry; int alpha, x0, y0, x1, y1, flip_u, flip_v; const uint8_t *sprite; } Puff;

static float ramp(float v, float lo, float hi) { return v <= lo ? 0.0f : v >= hi ? 1.0f : (v - lo) / (hi - lo); }

/* one kind's puffs of the group: their maximum, set against the frame's map, drawn as one sprite. Only the pixels the puffs touch are visited (each row's extent), not the union box of the group */
static void compose(int kind, const Puff *puffs, int count)
{
    int x0 = H.target->clip_right, y0 = H.target->clip_bottom, x1 = H.target->clip_left, y1 = H.target->clip_top;
    for (int i = 0; i < count; i++) {
        if (puffs[i].x0 < x0) x0 = puffs[i].x0;
        if (puffs[i].y0 < y0) y0 = puffs[i].y0;
        if (puffs[i].x1 > x1) x1 = puffs[i].x1;
        if (puffs[i].y1 > y1) y1 = puffs[i].y1;
    }
    const int w = x1 - x0, h = y1 - y0;
    if (count == 0 || w <= 0 || h <= 0 || chunk_count >= MAX_CHUNKS) return;
    const int ox = H.target->clip_left, oy = H.target->clip_top;
    for (int y = y0; y < y1; y++) row_lo[y - oy] = 1 << 30, row_hi[y - oy] = -1, fm_used[(size_t)KINDS * (size_t)map_h + (size_t)(y - oy)] = 0, fm_used[(size_t)(KINDS + 1) * (size_t)map_h + (size_t)(y - oy)] = 0;
    for (int y = (y0 - oy) / 2 > 0 ? (y0 - oy) / 2 - 1 : 0; y <= (y1 - 1 - oy) / 2 + 1 && y <= map_h / 2; y++) half_lo[y] = 1 << 30, half_hi[y] = -1;
    for (int i = 0; i < count; i++) {
        const Puff *p = &puffs[i];
        const uint8_t *sp = p->sprite;
        static int col[MAX_PUFF_W];   /* a puff is clipped to the target: no wider than it */
        uint8_t mul[256];
        const int pw = p->x1 - p->x0;
        if (pw > MAX_PUFF_W) continue;
        for (int a = 0; a < 256; a++) mul[a] = (uint8_t)(a * p->alpha / 255);
        if (p->rx > (float)HALF_RX) {   /* a big puff is smooth: its alpha is made on cells of 2x2 pixels (a quarter of the work), the dither below stays per pixel */
            const int hx0 = (p->x0 - ox) / 2, hx1 = (p->x1 - 1 - ox) / 2 + 1;
            for (int hx = hx0; hx < hx1; hx++) col[hx - hx0] = cell(((float)(ox + hx * 2 + 1) - p->cx) / p->rx, p->flip_u);
            const int tmin = (255 + p->alpha - 1) / p->alpha;   /* a texel gives the puff's map a value only from this: the alpha scaled down is 0 below it */
            int va = hx0, vb = hx1 - 1;   /* the cells inside the sprite (the others hold -1, at either end) */
            while (va <= vb && col[va - hx0] < 0) va++;
            while (vb >= va && col[vb - hx0] < 0) vb--;
            for (int hy = (p->y0 - oy) / 2; hy <= (p->y1 - 1 - oy) / 2; hy++) {
                const int r = cell(((float)(oy + hy * 2 + 1) - p->cy) / p->ry, p->flip_v);
                if (r < 0) continue;
                for (int y = oy + hy * 2; y < oy + hy * 2 + 2; y++)
                    if (y >= p->y0 && y < p->y1) fm_used[(size_t)(KINDS + 1) * (size_t)map_h + (size_t)(y - oy)] = 1;   /* the row is the puff's, however much of it draws */
                const uint8_t *src = sp + r * SPRAY_V4_ART_SIZE;
                int t0 = 0, t1 = SPRAY_V4_ART_SIZE - 1;   /* the sprite row's drawing texels: the cells outside them are 0 and are not visited */
                while (t0 <= t1 && src[t0] < tmin) t0++;
                while (t1 >= t0 && src[t1] < tmin) t1--;
                if (va > vb || t0 > t1) continue;
                int ca = va, cb = vb + 1;   /* the cells whose texel is in t0..t1: col is monotone over va..vb */
                if (!p->flip_u) {
                    int lo = va, hi = vb + 1;
                    while (lo < hi) { const int m = (lo + hi) / 2; if (col[m - hx0] >= t0) hi = m; else lo = m + 1; }
                    ca = lo, lo = ca, hi = vb + 1;
                    while (lo < hi) { const int m = (lo + hi) / 2; if (col[m - hx0] > t1) hi = m; else lo = m + 1; }
                    cb = lo;
                } else {
                    int lo = va, hi = vb + 1;
                    while (lo < hi) { const int m = (lo + hi) / 2; if (col[m - hx0] <= t1) hi = m; else lo = m + 1; }
                    ca = lo, lo = ca, hi = vb + 1;
                    while (lo < hi) { const int m = (lo + hi) / 2; if (col[m - hx0] < t0) hi = m; else lo = m + 1; }
                    cb = lo;
                }
                if (ca >= cb) continue;
                uint8_t *g = &half_map[(size_t)hy * (size_t)half_w];
                for (int hx = ca; hx < cb; hx++) {
                    const uint8_t a = mul[src[col[hx - hx0]]];
                    if (a > g[hx]) g[hx] = a;
                }
                if (ca < half_lo[hy]) half_lo[hy] = ca;
                if (cb > half_hi[hy]) half_hi[hy] = cb;
                for (int y = oy + hy * 2; y < oy + hy * 2 + 2; y++) {
                    if (y < p->y0 || y >= p->y1) continue;
                    const int lo = ox + ca * 2 > p->x0 ? ox + ca * 2 : p->x0, hi = ox + cb * 2 < p->x1 ? ox + cb * 2 : p->x1;
                    if (lo < row_lo[y - oy]) row_lo[y - oy] = lo;
                    if (hi > row_hi[y - oy]) row_hi[y - oy] = hi;
                }
            }
            continue;
        }
        for (int x = p->x0; x < p->x1; x++) col[x - p->x0] = cell(((float)x + 0.5f - p->cx) / p->rx, p->flip_u);
        for (int y = p->y0; y < p->y1; y++) {
            const int r = cell(((float)y + 0.5f - p->cy) / p->ry, p->flip_v);
            if (r < 0) continue;
            const uint8_t *src = sp + r * SPRAY_V4_ART_SIZE;
            uint8_t *g = &group_map[(size_t)(y - oy) * (size_t)map_w];
            for (int x = p->x0; x < p->x1; x++) {
                const int c = col[x - p->x0];
                if (c < 0) continue;
                const uint8_t a = mul[src[c]];
                if (a > g[x - ox]) g[x - ox] = a;
            }
            fm_used[(size_t)KINDS * (size_t)map_h + (size_t)(y - oy)] = 1;   /* the row has a small puff in the full map */
            if (p->x0 < row_lo[y - oy]) row_lo[y - oy] = p->x0;
            if (p->x1 > row_hi[y - oy]) row_hi[y - oy] = p->x1;
        }
    }
    /* the sprites are made in bands of rows, each as wide as the puffs on its rows are: a group's one box would be mostly empty (and the blend walks every texel of it) */
    const int per = LOOKS[kind].per;
    for (int by = y0; by < y1; by += BAND) {
        const int ey = by + BAND < y1 ? by + BAND : y1;
        int lo = 1 << 30, hi = -1;
        for (int y = by; y < ey; y++) {
            if (row_lo[y - oy] < lo) lo = row_lo[y - oy];
            if (row_hi[y - oy] > hi) hi = row_hi[y - oy];
        }
        const int bw = hi - lo, bh = ey - by;
        uint8_t *shape = bw > 0 && chunk_count < MAX_CHUNKS ? arena_zeroed((size_t)16 + (size_t)bw * (size_t)bh) : NULL;
        int any = 0;
        for (int y = by; y < ey; y++) {
            uint8_t *g = &group_map[(size_t)(y - oy) * (size_t)map_w], *fm = &frame_map[kind][(size_t)(y - oy) * (size_t)map_w];
            const uint8_t *hm = &half_map[(size_t)((y - oy) >> 1) * (size_t)half_w];
            uint8_t *out = shape != NULL ? shape + 16 + (size_t)(y - by) * (size_t)bw - lo : NULL;
            const int xe = row_hi[y - oy];
            if (fm_used[(size_t)(KINDS + 1) * (size_t)map_h + (size_t)(y - oy)]) fm_used[(size_t)kind * (size_t)map_h + (size_t)(y - oy)] |= xe > row_lo[y - oy] ? 0 : 1;   /* a big puff's row with nothing left to draw still counts as touched (the row's path stays what it was) */
            if (xe <= row_lo[y - oy]) continue;   /* no puff on this row */
            if (out == NULL) { for (int x = row_lo[y - oy]; x < xe; x++) g[x - ox] = 0; continue; }
            if (!fm_used[(size_t)kind * (size_t)map_h + (size_t)(y - oy)] && !fm_used[(size_t)KINDS * (size_t)map_h + (size_t)(y - oy)]) {   /* only big puffs and nothing before: the level is a table lookup per half-map cell and phase of the dither */
                const int xs = row_lo[y - oy];
                const uint8_t (*lv)[256] = LEVEL_AT[kind][y & 3];
                uint8_t *o = out + xs, *fp = &fm[xs - ox];
                const uint8_t *hp = &hm[(xs - ox) >> 1];
                const int n = xe - xs, lead = (xs - ox) & 1;
                unsigned m = 0;
                int i = 0;
                if (lead && n > 0) { const unsigned hv = hp[0]; fp[0] = (uint8_t)hv, o[0] = lv[xs & 3][hv], m |= o[0]; i = 1, hp++; }
                for (; i + 1 < n; i += 2, hp++) {
                    const unsigned hv = *hp;
                    const int p = (xs + i) & 3;
                    fp[i] = fp[i + 1] = (uint8_t)hv;
                    o[i] = lv[p][hv], o[i + 1] = lv[(p + 1) & 3][hv];
                    m |= o[i] | o[i + 1];
                }
                if (i < n) { const unsigned hv = *hp; fp[i] = (uint8_t)hv, o[i] = lv[(xs + i) & 3][hv], m |= o[i]; }
                any |= m != 0;
                fm_used[(size_t)kind * (size_t)map_h + (size_t)(y - oy)] = 1;
                continue;
            }
            if (!fm_used[(size_t)kind * (size_t)map_h + (size_t)(y - oy)]) {   /* nothing before: no pixel's `before` to subtract, and the row is made with no table */
                const int xs = row_lo[y - oy];
                const unsigned dr[4] = { (unsigned)DITHER[(y & 3) * 4] >> 8, (unsigned)DITHER[(y & 3) * 4 + 1] >> 8, (unsigned)DITHER[(y & 3) * 4 + 2] >> 8, (unsigned)DITHER[(y & 3) * 4 + 3] >> 8 };
                uint8_t *o = out + xs, *gp = &g[xs - ox], *fp = &fm[xs - ox];
                const uint8_t *hp = &hm[(xs - ox) >> 1];
                const int n = xe - xs, lead = (xs - ox) & 1;   /* a cell covers two pixels: the pairs start on an even one */
                unsigned m = 0;
                #define PIXEL(i, hv) do { const unsigned a = gp[i] > (hv) ? gp[i] : (hv), k = (a * (unsigned)per + dr[(xs + (i)) & 3]) >> 8; gp[i] = 0, fp[i] = (uint8_t)a; o[i] = (uint8_t)(k > LEVELS ? LEVELS : k); m |= k; } while (0)
                int i = 0;
                if (lead && n > 0) { PIXEL(0, hp[0]); i = 1, hp++; }
                for (; i + 1 < n; i += 2, hp++) { const unsigned hv = *hp; PIXEL(i, hv); PIXEL(i + 1, hv); }
                if (i < n) PIXEL(i, *hp);
                #undef PIXEL
                any |= m != 0;
                fm_used[(size_t)kind * (size_t)map_h + (size_t)(y - oy)] = 1;
                continue;
            }
            fm_used[(size_t)kind * (size_t)map_h + (size_t)(y - oy)] = 1;
            for (int x = row_lo[y - oy]; x < xe; x++) {
                const int hv = hm[(x - ox) >> 1], now = g[x - ox] > hv ? g[x - ox] : hv, before = fm[x - ox];
                g[x - ox] = 0;   /* the group's map is left clean for the next */
                const int d = now > before ? now - before : 0;   /* what the pixel still takes after what the groups before gave it: d * 255 / (255 - before) of 255 */
                int k = (d * per * RCP[before] + DITHER[(y & 3) * 4 + (x & 3)]) >> 16;
                k = k > LEVELS ? LEVELS : k;
                fm[x - ox] = (uint8_t)(now > before ? now : before);
                out[x] = (uint8_t)k, any |= k;
            }
        }
        if (shape == NULL) continue;
        if (!any) continue;
        shape[0] = 0x7b, shape[4] = (uint8_t)(bw & 0xff), shape[5] = (uint8_t)(bw >> 8), shape[6] = (uint8_t)(bh & 0xff), shape[7] = (uint8_t)(bh >> 8);
        chunk_count++;
        sprite_scaled_blend(H.target, rows[kind], shape, bw, bh, lo, by);
    }
    for (int hy = (y0 - oy) / 2 > 0 ? (y0 - oy) / 2 - 1 : 0; hy <= (y1 - 1 - oy) / 2 + 1 && hy <= map_h / 2; hy++)   /* the half map is left clean too */
        if (half_hi[hy] > half_lo[hy]) memset(&half_map[(size_t)hy * (size_t)half_w + (size_t)half_lo[hy]], 0, (size_t)(half_hi[hy] - half_lo[hy]));
}

void spray_v4_draw_row(int32_t node, int far_side)
{
    if (H.target == NULL || H.race == NULL || map_w == 0) return;
    static Puff puffs[KINDS][SPRAY_V4_MAX];
    int count[KINDS] = { 0 };
    const uint32_t tick = (uint32_t)H.race->tick;
    const int32_t focal = abs(H.view->view->focal_x);
    const int open = H.rain_open(node);
    for (int i = node_head[(uint32_t)node & (NODE_BUCKETS - 1)]; i >= 0; i = node_next[i]) {
        const SprayV4Particle *p = &state.p[i];
        SprayV4Look look;
        if (!p->live || p->node != node || !spray_v4_look(p, tick, open, &look)) continue;
        const int32_t place[3] = { (int32_t)(look.x * 65536.0f), (int32_t)(look.y * 65536.0f), (int32_t)(look.z * 65536.0f) };
        int32_t camera[3], screen[2];
        if (!H.world_screen(place, camera, screen) || camera[2] < 64) continue;
        const Car *owner = p->car < RACE_CARS ? H.race->cars[p->car] : NULL;
        int32_t own_camera[3], own_screen[2];
        const int32_t own_at[3] = { owner != NULL ? owner->x : 0, owner != NULL ? owner->y : 0, owner != NULL ? owner->z : 0 };
        const int farther = owner == NULL || !H.world_screen(own_at, own_camera, own_screen) || camera[2] >= own_camera[2];
        if (farther != far_side) continue;
        const Look *lk = &LOOKS[p->kind];
        const float metres = (float)camera[2] / 32.0f;   /* a camera unit is 1/32 m */
        const float fade = ramp(metres, lk->near0, lk->near1) * (1.0f - ramp(metres, lk->far0, lk->far1));
        float keep = 1.0f;
        if (owner != NULL && !far_side && own_camera[2] > 64) {   /* a plume between the camera and its own car does not fog the body: over the car's box it keeps a fraction */
            const float unit_px = 32.0f * (float)focal / (float)own_camera[2];   /* pixels in a metre at the car's depth */
            const float half = (owner->model_width > 0 ? (float)owner->model_width / 65536.0f : 1.8f) * 0.5f * unit_px;
            const float dx = (float)(screen[0] - own_screen[0]), dy = (float)(screen[1] - own_screen[1]);
            if (fabsf(dx) < half && dy > -1.6f * unit_px && dy < 0.4f * unit_px) keep = BODY_KEEP;
        }
        const int alpha = (int)(look.alpha * fade * keep * 255.0f);
        if (alpha < 3) continue;
        const float rpx = (float)(((int32_t)(look.radius * 65536.0f)) >> WORLD_SHIFT) * (float)focal / (float)camera[2];
        const uint32_t pick = (uint32_t)(i * 7) + p->born / 2;
        const int variant = (int)(pick % SPRAY_V4_ART_VARIANTS), flip_u = (int)(pick / 5 & 1), flip_v = 0;   /* every sprite a billboard: square, facing the camera */
        Puff *q = &puffs[p->kind][count[p->kind]];
        q->cx = (float)screen[0], q->rx = (rpx < 1.0f ? 1.0f : rpx) * lk->scale;
        q->ry = q->rx * lk->flat, q->cy = (float)screen[1] - q->ry * lk->lift;
        q->sprite = spray_v4_art(lk->family, variant);
        q->flip_u = flip_u, q->flip_v = flip_v;
        q->alpha = alpha > 255 ? 255 : alpha;
        q->x0 = (int)floorf(q->cx - q->rx), q->x1 = (int)ceilf(q->cx + q->rx), q->y0 = (int)floorf(q->cy - q->ry), q->y1 = (int)ceilf(q->cy + q->ry);
        if (q->x0 < H.target->clip_left) q->x0 = H.target->clip_left;
        if (q->y0 < H.target->clip_top) q->y0 = H.target->clip_top;
        if (q->x1 > H.target->clip_right) q->x1 = H.target->clip_right;
        if (q->y1 > H.target->clip_bottom) q->y1 = H.target->clip_bottom;
        if (q->x0 >= q->x1 || q->y0 >= q->y1) continue;
        count[p->kind]++;
        if (p->kind == SPRAY_V4_GRAVEL && q->rx >= 1.6f) {   /* the stone's glint: the same place, its own sprite, the light colour */
            Puff *g = &puffs[GLINT][count[GLINT]];
            *g = *q, g->sprite = spray_v4_art_glint(lk->family, variant);
            if (g->sprite != NULL) count[GLINT]++;
        }
    }
    for (int kind = 0; kind < KINDS; kind++) compose(kind, puffs[kind], count[kind]);
}
