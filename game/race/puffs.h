/* The tyre smoke's puffs (00456948, 00456820): the additive blend their
 * sprites draw through. */
#ifndef TNFS_PUFFS_H
#define TNFS_PUFFS_H

#include "game/gfx/quad.h"
#include "game/race/crash_trail.h"
#include "game/race/race_palette.h"

#include <stdint.h>

enum { PUFF_BLEND_SIZE = 256 * 256 };

/* 00456820 (watcall (table, palette)): palette 256 x 4 bytes (three channels
 * 0..255 then a spare). table[a x 256 + b] = nearest(min(pal[b] + pal[a] -
 * pal[a] / 4, 255) per channel), the channels packed c0 | c1 << 8 | c2 << 16
 * (00473140). Row a is the sprite's texel, column b what lies under it. */
void puff_blend_build(uint8_t table[PUFF_BLEND_SIZE], const uint8_t palette[1024],
                      uint8_t (*nearest)(void *ctx, uint32_t rgb), void *ctx);

enum { PUFF_BLEND_ROWS = 256 };

typedef struct {
    uint8_t palette[1024];                     /* 00513e98: 4 bytes a colour, the 4th 0x80 (the last 0) */
    const uint8_t *rows[PUFF_BLEND_ROWS];      /* 005142a8: table + a x 256 (*004c795c, 0048c88d) */
} PuffBlend;

/* 00456948: the screen palette read as 6-bit (004766b8), each channel back
 * to v x 255 / 63 in the screen's order (the read's R, G, B as bytes 2, 1,
 * 0), the 4th byte 0x80 and the last entry's 0; then table (the original
 * allocates it, 'lut' 0x10000 at 005146a8) built from it (00456820) and the
 * rows pointed into it. */
void puff_blend_init(PuffBlend *b, uint8_t table[PUFF_BLEND_SIZE], const RacePalette *p,
                     uint8_t (*nearest)(void *ctx, uint32_t rgb), void *ctx);

enum { PUFF_SHAPES = 4 };

typedef struct {
    uint8_t *bank;                  /* 004c55dc: smoke.qfs, read once */
    uint8_t *smok;                  /* 004c55e0 */
    uint8_t *palette;               /* 004c55e4: '!PAL' + 0x10, 6-bit, tinted */
    uint8_t *smoke[PUFF_SHAPES];    /* 00513e88: 'smo1'..'smo4' */
    uint8_t *dirt[PUFF_SHAPES];     /* 00514298: 'dir1'..'dir4' */
} PuffArt;

/* 00456a7c (eax = 005146ac, from 00455b74 only): unless the bank is in,
 * dir (005246e8) + "smoke.qfs" read (00476268), its shapes found (004732dd
 * for 'smok', 004732e4 for the rest), the palette tinted (004471dc, 0 ..
 * 0x100) and queued with the bank (00446f58, from 0, 0xfa colours); then
 * the trail's +0x640 words k (their index) and its ages 0xffff0000, and the trail cleared
 * (00456a48's body, inline). The original faults on a bank with no '!PAL'
 * and aborts with no 'smok'; the port leaves the palette out then. */
void puff_art_start(PuffArt *art, CrashTrail *trail, RacePalette *p, const char *dir,
                    uint8_t *(*load)(void *ctx, const char *path), void *ctx);

typedef struct {
    void *ctx;
    void (*free)(void *ctx, void *block);   /* 00475260 */
} PuffFreeOps;

/* 00456c84 (eax = 005146ac, unread; from 00455a5c's teardown): the bank
 * freed whatever it holds, the blend table (005146a8, 'lut') freed and
 * cleared when held, then the bank cleared. */
void puff_art_free(const PuffFreeOps *ops, PuffArt *art, uint8_t **table);

typedef struct {
    const QuadTarget *target;
    const uint8_t *const *blend;              /* *004c795c: 256 rows */
    const uint8_t *smoke[PUFF_SHAPES];        /* 00513e88: 'smo1'..'smo4' */
    const uint8_t *dirt[PUFF_SHAPES];         /* 00514298: 'dir1'..'dir4' */
    int32_t high_res;                         /* 004fa0c0 */
    void *ctx;
    int32_t (*rand_below)(void *ctx, int32_t n);  /* 00443730 */
} PuffView;

/* 00456bd4 (stdcall x, y, size): smoke[2 x rand(2)] at (x, y), scaled by
 * size (halved, toward zero, at low resolution), blended (00490570). */
void puff_smoke(const PuffView *v, int32_t x, int32_t y, int32_t size);

/* 00456c28 (stdcall x, y, size): dirt[rand(3)], size halved likewise then at
 * most 0x10000, colour 0xff left out (00490b00). */
void puff_dirt(const PuffView *v, int32_t x, int32_t y, int32_t size);

#endif
