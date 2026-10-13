/* The tyre smoke's puffs (00456820). */
#include "game/race/puffs.h"

#include "game/gfx/sprite_scaled.h"
#include "game/sys/shape_bank.h"

#include <stdio.h>

enum { CHANNEL_MAX = 0xff, DIRT_MOST = 0x10000, SIX_BIT_MAX = 0x3f, SPARE = 0x80 };

static int32_t lift(int32_t under, int32_t added)
{
    const int32_t c = under + added;
    return c > CHANNEL_MAX ? CHANNEL_MAX : c;
}

void puff_blend_build(uint8_t table[PUFF_BLEND_SIZE], const uint8_t palette[1024],
                      uint8_t (*nearest)(void *ctx, uint32_t rgb), void *ctx)
{
    for (int a = 0; a < 256; a++) {
        const uint8_t *top = palette + 4 * a;
        const int32_t add0 = top[0] - top[0] / 4, add1 = top[1] - top[1] / 4, add2 = top[2] - top[2] / 4;
        for (int b = 0; b < 256; b++) {
            const uint8_t *under = palette + 4 * b;
            const uint32_t rgb = (uint32_t)lift(under[0], add0) | (uint32_t)lift(under[1], add1) << 8 |
                                 (uint32_t)lift(under[2], add2) << 16;
            table[a * 256 + b] = nearest(ctx, rgb);
        }
    }
}

void puff_blend_init(PuffBlend *b, uint8_t table[PUFF_BLEND_SIZE], const RacePalette *p,
                     uint8_t (*nearest)(void *ctx, uint32_t rgb), void *ctx)
{
    uint8_t six[3 * PUFF_BLEND_ROWS];
    race_palette_read(p, 0, PUFF_BLEND_ROWS, six);
    for (int i = 0; i < PUFF_BLEND_ROWS; i++) {
        for (int c = 0; c < 3; c++)
            b->palette[4 * i + 2 - c] = (uint8_t)(six[3 * i + c] * CHANNEL_MAX / SIX_BIT_MAX);
        b->palette[4 * i + 3] = SPARE;
    }
    b->palette[sizeof b->palette - 1] = 0;
    puff_blend_build(table, b->palette, nearest, ctx);
    for (int a = 0; a < PUFF_BLEND_ROWS; a++)
        b->rows[a] = table + a * 256;
}

enum { ART_PATH = 0x50, SHAPE_HEADER = 0x10, TINT_END = 0x100, BANK_COLOURS = 0xfa, AGE_UNSET = (int32_t)0xffff0000 };

static const uint8_t SMOK[4] = { 's', 'm', 'o', 'k' }, PAL[4] = { '!', 'P', 'A', 'L' };
static const uint8_t SMOKE_NAMES[PUFF_SHAPES][4] = { { 's', 'm', 'o', '1' }, { 's', 'm', 'o', '2' }, { 's', 'm', 'o', '3' }, { 's', 'm', 'o', '4' } };
static const uint8_t DIRT_NAMES[PUFF_SHAPES][4] = { { 'd', 'i', 'r', '1' }, { 'd', 'i', 'r', '2' }, { 'd', 'i', 'r', '3' }, { 'd', 'i', 'r', '4' } };

static void puff_art_load(PuffArt *art, RacePalette *p, const char *dir,
                          uint8_t *(*load)(void *ctx, const char *path), void *ctx)
{
    char path[ART_PATH];
    snprintf(path, sizeof path, "%s%s", dir, "smoke.qfs");
    art->bank = load(ctx, path);
    if (art->bank == NULL) return;
    art->smok = shape_bank_find(art->bank, SMOK);
    for (int k = 0; k < PUFF_SHAPES; k++) {
        art->smoke[k] = shape_bank_find(art->bank, SMOKE_NAMES[k]);
        art->dirt[k] = shape_bank_find(art->bank, DIRT_NAMES[k]);
    }
    uint8_t *pal = shape_bank_find(art->bank, PAL);
    art->palette = pal != NULL ? pal + SHAPE_HEADER : NULL;
    if (art->palette == NULL) return;
    race_palette_tint(p, 0, TINT_END, art->palette);
    race_palette_queue_bank(p, art->bank, art->palette, 0, BANK_COLOURS);
}

void puff_art_start(PuffArt *art, CrashTrail *trail, RacePalette *p, const char *dir,
                    uint8_t *(*load)(void *ctx, const char *path), void *ctx)
{
    if (art->bank == NULL) puff_art_load(art, p, dir, load, ctx);
    for (int k = 0; k < CRASH_TRAIL_POINTS; k++) {
        trail->unknown_640[k] = k;
        trail->age[k] = AGE_UNSET;
    }
    crash_trail_clear(trail);
}

static int32_t at_resolution(const PuffView *v, int32_t size)
{
    return v->high_res != 0 ? size : size / 2;
}

void puff_smoke(const PuffView *v, int32_t x, int32_t y, int32_t size)
{
    const int32_t pick = v->rand_below(v->ctx, 2) * 2;
    sprite_scaled_blend_at(v->target, v->blend, v->smoke[pick], at_resolution(v, size), x, y);
}

void puff_dirt(const PuffView *v, int32_t x, int32_t y, int32_t size)
{
    const int32_t pick = v->rand_below(v->ctx, 3);
    size = at_resolution(v, size);
    if (size > DIRT_MOST) size = DIRT_MOST;
    sprite_scaled_at(v->target, v->dirt[pick], size, x, y);
}
