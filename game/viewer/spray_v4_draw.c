#include "viewer/spray_v4_draw.h"

#include "game/gfx/sprite_scaled.h"
#include "game/race/spray_tint.h"
#include "viewer/spray_v4_art.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* the drawn kinds: the particles' own (SprayV4Kind) and the stones' glint */
enum { WORLD_SHIFT = 11, MAX_CHUNKS = 512, LEVELS = 8, GLINT = SPRAY_V4_KINDS, KINDS = SPRAY_V4_KINDS + 1 };

/* how each kind is drawn: its sprite family, its colour's cap (a level k mixes k/per toward it), how far the sprite's half width is from the particle's radius (the body of the sprite is a fraction of it), its height over
 * its width, how far it sits above the road, and the distances it fades in (from `near0` to `near1` metres) and out (`far0` to `far1`) */
typedef struct { int family; int per; float scale, flat, lift, near0, near1, far0, far1; } Look;
static const Look LOOKS[KINDS] = {
    [SPRAY_V4_MIST]   = { SPRAY_V4_ART_MIST,      16, 1.7f, 0.55f, 0.30f, 0.8f, 4.0f, 80.0f, 300.0f },
    [SPRAY_V4_GRAVEL] = { SPRAY_V4_ART_STONE,     12, 1.5f, 1.00f, 0.00f, 0.8f, 2.5f, 30.0f, 90.0f },
    [SPRAY_V4_DUST]   = { SPRAY_V4_ART_FINE_DUST, 20, 1.5f, 0.80f, 0.30f, 0.8f, 4.0f, 80.0f, 300.0f },
    [SPRAY_V4_DROP]   = { SPRAY_V4_ART_DROP,      10, 2.4f, 1.00f, 0.00f, 0.5f, 2.0f, 40.0f, 120.0f },
    [SPRAY_V4_EARTH]  = { SPRAY_V4_ART_EARTH,      8, 2.0f, 0.80f, 0.30f, 0.8f, 4.0f, 80.0f, 300.0f },
    [SPRAY_V4_SNOW]   = { SPRAY_V4_ART_SNOW,      11, 1.5f, 0.90f, 0.30f, 0.8f, 4.0f, 80.0f, 300.0f },
    [SPRAY_V4_CLUMP]  = { SPRAY_V4_ART_CLUMP,     10, 2.2f, 1.00f, 0.00f, 0.8f, 2.5f, 40.0f, 120.0f },
    [GLINT]           = { SPRAY_V4_ART_STONE,      8, 1.5f, 1.00f, 0.00f, 0.8f, 2.5f, 30.0f, 90.0f }
};
static const float BAYER[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };

static SprayV4 state;
static SprayV4Host H;
static uint8_t identity[256];
static uint8_t tables[KINDS][LEVELS + 1][256];
static const uint8_t *rows[KINDS][256];
static uint8_t made_palette[768], made_rgb[KINDS][3];
static int tables_made;
static uint8_t *frame_map[KINDS], *group_map;
static size_t frame_cap, group_cap;
static int map_w, map_h;
static uint8_t *chunks[MAX_CHUNKS];
static int chunk_count;

SprayV4 *spray_v4_state(void) { return &state; }

/* the sprite's alpha (0..255) at the unit-square point (u, v) in -1..1, bilinear */
static int sample(const uint8_t *sp, float u, float v)
{
    const float fx = (u + 1.0f) * 0.5f * (float)(SPRAY_V4_ART_SIZE - 1), fy = (v + 1.0f) * 0.5f * (float)(SPRAY_V4_ART_SIZE - 1);
    int x0 = (int)fx, y0 = (int)fy;
    if (x0 >= SPRAY_V4_ART_SIZE - 1) x0 = SPRAY_V4_ART_SIZE - 2;
    if (y0 >= SPRAY_V4_ART_SIZE - 1) y0 = SPRAY_V4_ART_SIZE - 2;
    const float tx = fx - (float)x0, ty = fy - (float)y0;
    const float a = (float)sp[y0 * SPRAY_V4_ART_SIZE + x0], b = (float)sp[y0 * SPRAY_V4_ART_SIZE + x0 + 1];
    const float c = (float)sp[(y0 + 1) * SPRAY_V4_ART_SIZE + x0], d = (float)sp[(y0 + 1) * SPRAY_V4_ART_SIZE + x0 + 1];
    return (int)((a * (1.0f - tx) + b * tx) * (1.0f - ty) + (c * (1.0f - tx) + d * tx) * ty);
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
    for (int i = 0; i < chunk_count; i++) free(chunks[i]);
    chunk_count = 0;
    map_w = H.target->clip_right - H.target->clip_left, map_h = H.target->clip_bottom - H.target->clip_top;
    if (map_w <= 0 || map_h <= 0) { map_w = map_h = 0; return; }
    const size_t need = (size_t)map_w * (size_t)map_h;
    if (need > frame_cap) {
        int ok = 1;
        for (int k = 0; k < KINDS; k++) { free(frame_map[k]); frame_map[k] = calloc(need, 1), ok &= frame_map[k] != NULL; }
        free(group_map), group_map = calloc(need, 1);
        frame_cap = group_cap = need;
        if (!ok || group_map == NULL) { frame_cap = group_cap = 0, map_w = map_h = 0; return; }
    }
    for (int k = 0; k < KINDS; k++) memset(frame_map[k], 0, need);
}

typedef struct { float cx, cy, rx, ry; int alpha, x0, y0, x1, y1, flip_u, flip_v; const uint8_t *sprite; } Puff;

static float ramp(float v, float lo, float hi) { return v <= lo ? 0.0f : v >= hi ? 1.0f : (v - lo) / (hi - lo); }

/* one kind's puffs of the group: their maximum, set against the frame's map, drawn as one sprite */
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
    uint8_t *shape = calloc((size_t)16 + (size_t)w * (size_t)h, 1);
    if (shape == NULL) return;
    const int ox = H.target->clip_left, oy = H.target->clip_top;
    for (int y = y0; y < y1; y++) memset(group_map + (size_t)(y - oy) * (size_t)map_w + (size_t)(x0 - ox), 0, (size_t)w);
    for (int i = 0; i < count; i++) {
        const Puff *p = &puffs[i];
        const uint8_t *sp = p->sprite;
        for (int y = p->y0; y < p->y1; y++) {
            const float v = ((float)y + 0.5f - p->cy) / p->ry;
            for (int x = p->x0; x < p->x1; x++) {
                const float u = ((float)x + 0.5f - p->cx) / p->rx;
                if (u <= -1.0f || u >= 1.0f || v <= -1.0f || v >= 1.0f) continue;
                const int a = sample(sp, p->flip_u ? -u : u, p->flip_v ? -v : v) * p->alpha / 255;
                uint8_t *g = &group_map[(size_t)(y - oy) * (size_t)map_w + (size_t)(x - ox)];
                if (a > *g) *g = (uint8_t)a;
            }
        }
    }
    const int per = LOOKS[kind].per;
    int any = 0;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            const size_t at = (size_t)(y - oy) * (size_t)map_w + (size_t)(x - ox);
            const int now = group_map[at], before = frame_map[kind][at];
            if (now <= before) continue;
            const float inc = (float)(now - before) * 255.0f / (float)(255 - before);   /* what the pixel still takes after what the groups before gave it */
            int k = (int)(inc * (float)per / 255.0f + (BAYER[(y & 3) * 4 + (x & 3)] + 0.5f) / 16.0f);
            if (k > LEVELS) k = LEVELS;
            frame_map[kind][at] = (uint8_t)now;
            if (k > 0) shape[16 + (size_t)(y - y0) * (size_t)w + (size_t)(x - x0)] = (uint8_t)k, any = 1;
        }
    if (!any) { free(shape); return; }
    shape[0] = 0x7b, shape[4] = (uint8_t)(w & 0xff), shape[5] = (uint8_t)(w >> 8), shape[6] = (uint8_t)(h & 0xff), shape[7] = (uint8_t)(h >> 8);
    chunks[chunk_count++] = shape;   /* kept until the next frame: the recorder replays the sprite when the frame's 3D is whole */
    sprite_scaled_blend(H.target, rows[kind], shape, w, h, x0, y0);
}

void spray_v4_draw_row(int32_t node, int far_side)
{
    if (H.target == NULL || H.race == NULL || map_w == 0) return;
    static Puff puffs[KINDS][SPRAY_V4_MAX];
    int count[KINDS] = { 0 };
    const uint32_t tick = (uint32_t)H.race->tick;
    const int32_t focal = abs(H.view->view->focal_x);
    const int open = H.rain_open(node);
    for (int i = 0; i < SPRAY_V4_MAX; i++) {
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
        const int alpha = (int)(look.alpha * fade * 255.0f);
        if (alpha < 3) continue;
        const float rpx = (float)(((int32_t)(look.radius * 65536.0f)) >> WORLD_SHIFT) * (float)focal / (float)camera[2];
        const uint32_t pick = (uint32_t)(i * 7) + p->born / 2;
        int variant = (int)(pick % SPRAY_V4_ART_VARIANTS), flip_u = (int)(pick / 5 & 1), flip_v = 0;
        Puff *q = &puffs[p->kind][count[p->kind]];
        q->cx = (float)screen[0], q->rx = (rpx < 1.0f ? 1.0f : rpx) * lk->scale;
        q->ry = q->rx * lk->flat, q->cy = (float)screen[1] - q->ry * lk->lift;
        if (p->kind == SPRAY_V4_DROP) {   /* a drop is a tear along the way it flies on the screen: the sprite's head forward */
            SprayV4Look ahead;
            int32_t c2[3], s2[2];
            float dx = 0.0f, dy = 0.0f;
            if (spray_v4_look(p, tick + 2, open, &ahead)) {
                const int32_t to[3] = { (int32_t)(ahead.x * 65536.0f), (int32_t)(ahead.y * 65536.0f), (int32_t)(ahead.z * 65536.0f) };
                if (H.world_screen(to, c2, s2) && c2[2] >= 64) dx = (float)(s2[0] - screen[0]), dy = (float)(s2[1] - screen[1]);
            }
            const float len = sqrtf(dx * dx + dy * dy);
            if (len < 0.8f) variant = 0, flip_u = 0;   /* barely moving on the screen: a round drop */
            else {
                const float ax = fabsf(dx), ay = fabsf(dy);
                if (ay < 0.4f * ax) variant = 1, flip_u = dx < 0.0f;
                else if (ax < 0.4f * ay) variant = 3, flip_u = 0, flip_v = dy < 0.0f;   /* the vertical tear's head is at +y */
                else if (len > 6.0f && dx * dy < 0.0f) variant = 4, flip_u = dx < 0.0f, flip_v = dy > 0.0f;
                else variant = 2, flip_u = dx < 0.0f, flip_v = dy < 0.0f;
            }
            q->rx *= 1.0f + 0.04f * (len > 12.0f ? 12.0f : len);   /* a fast drop is a longer streak */
            q->ry = q->rx * lk->flat, q->cy = (float)screen[1];
        }
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
