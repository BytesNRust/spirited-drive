#include "viewer/spray_v4_draw.h"

#include "game/gfx/sprite_scaled.h"
#include "game/race/spray_tint.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

enum { WORLD_SHIFT = 11, MAX_CHUNKS = 256, LEVELS = 8, KINDS = 2, SPRITE_BYTES = 16 + SPRAY_V4_SPRITE * SPRAY_V4_SPRITE };
static const int WATER_LEVELS_PER_255 = 16, GRAVEL_LEVELS_PER_255 = 12;   /* a level k mixes k/16 (water, at most a half) or k/12 (gravel, two thirds) toward the colour */
static const float WATER_FLAT = 0.72f;   /* the puff's height over its width: a mist along the road */
static const float BAYER[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };

static SprayV4 state;
static SprayV4Host H;
static uint8_t sprites[SPRAY_V4_CLASSES + 1][SPRAY_V4_SPRITE * SPRAY_V4_SPRITE];
static int sprites_made;
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

/* ---- the puffs' sprites: a few lumps of different softness inside a window that is nil at the edge, so no hard rim; bigger classes have more lumps (a big puff is not a disc) ---- */
static void sprite_make(uint8_t *out, int lobes, float spread, float sigma, uint32_t seed)
{
    float cx[6], cy[6], sg[6];
    uint32_t r = seed * 2654435761u + 1u;
    for (int i = 0; i < lobes; i++) {
        r = r * 1664525u + 1013904223u, cx[i] = ((float)(r >> 16 & 0xff) / 255.0f - 0.5f) * 2.0f * spread;
        r = r * 1664525u + 1013904223u, cy[i] = ((float)(r >> 16 & 0xff) / 255.0f - 0.5f) * 2.0f * spread;
        r = r * 1664525u + 1013904223u, sg[i] = sigma * (0.75f + 0.5f * (float)(r >> 16 & 0xff) / 255.0f);
    }
    float best = 0.0f;
    static float tmp[SPRAY_V4_SPRITE * SPRAY_V4_SPRITE];
    for (int y = 0; y < SPRAY_V4_SPRITE; y++)
        for (int x = 0; x < SPRAY_V4_SPRITE; x++) {
            const float u = ((float)x + 0.5f) / 32.0f - 1.0f, v = ((float)y + 0.5f) / 32.0f - 1.0f, r2 = u * u + v * v;
            float s = 0.0f;
            for (int i = 0; i < lobes; i++) s += expf(-((u - cx[i]) * (u - cx[i]) + (v - cy[i]) * (v - cy[i])) / (2.0f * sg[i] * sg[i]));
            const float w = r2 >= 1.0f ? 0.0f : (1.0f - r2) * (1.0f - r2);
            tmp[y * SPRAY_V4_SPRITE + x] = s * w;
            if (tmp[y * SPRAY_V4_SPRITE + x] > best) best = tmp[y * SPRAY_V4_SPRITE + x];
        }
    for (int i = 0; i < SPRAY_V4_SPRITE * SPRAY_V4_SPRITE; i++) out[i] = (uint8_t)(best > 0.0f ? tmp[i] / best * 255.0f + 0.5f : 0.0f);
}

static void sprites_make(void)
{
    if (sprites_made) return;
    sprites_made = 1;
    for (int i = 0; i < 256; i++) identity[i] = (uint8_t)i;
    static const int LOBES[SPRAY_V4_CLASSES] = { 1, 2, 3, 4, 5 };
    for (int c = 0; c < SPRAY_V4_CLASSES; c++) sprite_make(sprites[c], LOBES[c], c == 0 ? 0.0f : 0.30f, 0.42f - 0.02f * (float)c, 11u + (uint32_t)c);
    sprite_make(sprites[SPRAY_V4_CLASSES], 1, 0.0f, 0.5f, 99u);
}

const uint8_t *spray_v4_sprite(int cls)
{
    sprites_make();
    return cls < 0 || cls > SPRAY_V4_CLASSES ? NULL : sprites[cls];
}

/* the sprite's alpha (0..255) at the unit-square point (u, v) in -1..1, bilinear */
static int sample(const uint8_t *sp, float u, float v)
{
    const float fx = (u + 1.0f) * 0.5f * (float)(SPRAY_V4_SPRITE - 1), fy = (v + 1.0f) * 0.5f * (float)(SPRAY_V4_SPRITE - 1);
    int x0 = (int)fx, y0 = (int)fy;
    if (x0 >= SPRAY_V4_SPRITE - 1) x0 = SPRAY_V4_SPRITE - 2;
    if (y0 >= SPRAY_V4_SPRITE - 1) y0 = SPRAY_V4_SPRITE - 2;
    const float tx = fx - (float)x0, ty = fy - (float)y0;
    const float a = (float)sp[y0 * SPRAY_V4_SPRITE + x0], b = (float)sp[y0 * SPRAY_V4_SPRITE + x0 + 1];
    const float c = (float)sp[(y0 + 1) * SPRAY_V4_SPRITE + x0], d = (float)sp[(y0 + 1) * SPRAY_V4_SPRITE + x0 + 1];
    return (int)((a * (1.0f - tx) + b * tx) * (1.0f - ty) + (c * (1.0f - tx) + d * tx) * ty);
}

static void tables_set(const uint8_t water_rgb[3], const uint8_t gravel_rgb[3])
{
    if (tables_made && memcmp(made_palette, H.palette, sizeof made_palette) == 0 && memcmp(made_rgb[0], water_rgb, 3) == 0 && memcmp(made_rgb[1], gravel_rgb, 3) == 0) return;
    memcpy(made_palette, H.palette, sizeof made_palette), memcpy(made_rgb[0], water_rgb, 3), memcpy(made_rgb[1], gravel_rgb, 3);
    for (int kind = 0; kind < KINDS; kind++) {
        const int per = kind == 0 ? WATER_LEVELS_PER_255 : GRAVEL_LEVELS_PER_255;
        memcpy(tables[kind][0], identity, 256);
        for (int k = 1; k <= LEVELS; k++) spray_tint_table(H.palette, made_rgb[kind], k * 100 / per, tables[kind][k]);
        for (int i = 0; i < 256; i++) rows[kind][i] = tables[kind][0];
        for (int k = 0; k <= LEVELS; k++) rows[kind][k] = tables[kind][k];
    }
    tables_made = 1;
}

void spray_v4_draw_frame(const SprayV4Host *host, const uint8_t water_rgb[3], const uint8_t gravel_rgb[3])
{
    H = *host;
    sprites_make();
    tables_set(water_rgb, gravel_rgb);
    for (int i = 0; i < chunk_count; i++) free(chunks[i]);
    chunk_count = 0;
    map_w = H.target->clip_right - H.target->clip_left, map_h = H.target->clip_bottom - H.target->clip_top;
    if (map_w <= 0 || map_h <= 0) { map_w = map_h = 0; return; }
    const size_t need = (size_t)map_w * (size_t)map_h;
    if (need > frame_cap) {
        for (int k = 0; k < KINDS; k++) { free(frame_map[k]); frame_map[k] = calloc(need, 1); }
        free(group_map), group_map = calloc(need, 1);
        frame_cap = group_cap = need;
        if (frame_map[0] == NULL || frame_map[1] == NULL || group_map == NULL) { frame_cap = group_cap = 0, map_w = map_h = 0; return; }
    }
    for (int k = 0; k < KINDS; k++) memset(frame_map[k], 0, need);
}

typedef struct { float cx, cy, rx, ry; int alpha, cls, x0, y0, x1, y1; } Puff;

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
        const uint8_t *sp = sprites[p->cls];
        for (int y = p->y0; y < p->y1; y++) {
            const float v = ((float)y + 0.5f - p->cy) / p->ry;
            for (int x = p->x0; x < p->x1; x++) {
                const float u = ((float)x + 0.5f - p->cx) / p->rx;
                if (u <= -1.0f || u >= 1.0f || v <= -1.0f || v >= 1.0f) continue;
                const int a = sample(sp, u, v) * p->alpha / 255;
                uint8_t *g = &group_map[(size_t)(y - oy) * (size_t)map_w + (size_t)(x - ox)];
                if (a > *g) *g = (uint8_t)a;
            }
        }
    }
    const int per = kind == 0 ? WATER_LEVELS_PER_255 : GRAVEL_LEVELS_PER_255;
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
    int count[KINDS] = { 0, 0 };
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
        const float metres = (float)camera[2] / 32.0f;   /* a camera unit is 1/32 m */
        const float fade = p->kind == SPRAY_V4_WATER ? ramp(metres, 1.5f, 8.0f) * (1.0f - ramp(metres, 80.0f, 300.0f)) : ramp(metres, 0.8f, 2.5f) * (1.0f - ramp(metres, 30.0f, 90.0f));
        const int alpha = (int)(look.alpha * fade * 255.0f);
        if (alpha < 3) continue;
        const float rpx = (float)(((int32_t)(look.radius * 65536.0f)) >> WORLD_SHIFT) * (float)focal / (float)camera[2];
        Puff *q = &puffs[p->kind][count[p->kind]];
        q->cx = (float)screen[0], q->rx = rpx < 1.5f ? 1.5f : rpx;
        q->cls = SPRAY_V4_CLASSES;
        if (p->kind == SPRAY_V4_WATER) {
            q->ry = q->rx * WATER_FLAT, q->cy = (float)screen[1] - q->ry * 0.3f;   /* it sits on the road: a little of it rises above its place */
            q->cls = q->rx < 10.0f ? 0 : q->rx < 22.0f ? 1 : q->rx < 45.0f ? 2 : q->rx < 90.0f ? 3 : 4;
        } else q->ry = q->rx, q->cy = (float)screen[1];
        q->alpha = alpha > 255 ? 255 : alpha;
        q->x0 = (int)floorf(q->cx - q->rx), q->x1 = (int)ceilf(q->cx + q->rx), q->y0 = (int)floorf(q->cy - q->ry), q->y1 = (int)ceilf(q->cy + q->ry);
        if (q->x0 < H.target->clip_left) q->x0 = H.target->clip_left;
        if (q->y0 < H.target->clip_top) q->y0 = H.target->clip_top;
        if (q->x1 > H.target->clip_right) q->x1 = H.target->clip_right;
        if (q->y1 > H.target->clip_bottom) q->y1 = H.target->clip_bottom;
        if (q->x0 >= q->x1 || q->y0 >= q->y1) continue;
        count[p->kind]++;
    }
    for (int kind = 0; kind < KINDS; kind++) compose(kind, puffs[kind], count[kind]);
}
