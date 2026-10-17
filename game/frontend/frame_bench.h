/* How fast the screen draws (00429278 picks config.dat's defaults by it). */
#ifndef TNFS_FRAME_BENCH_H
#define TNFS_FRAME_BENCH_H

#include "game/frontend/palette_put.h"

#include <stdint.h>

typedef struct FrameBenchOps {
    void (*pointer_hide)(void *ctx);                                     /* [004c618c] */
    void (*fill)(void *ctx, uint32_t colour);                            /* 00482980 (cdecl) */
    void (*unlock)(void *ctx);                                           /* 004733b0 */
    void (*flip)(void *ctx);                                             /* 004733ec */
    int32_t (*load_art)(void *ctx, const char *name);                    /* 00466c34: eax the name; the bank */
    const uint8_t *(*shape)(void *ctx, int32_t bank, const char *tag);   /* 004732dd (cdecl) */
    void (*select)(void *ctx, int32_t picture);                          /* 00473680 (cdecl) */
    void (*blit)(void *ctx, const uint8_t *shape);                       /* 004737ec (cdecl): at its own place */
    void (*select_default)(void *ctx);                                   /* 00473604 */
    void (*blit_at)(void *ctx, int32_t shape, int32_t x, int32_t y);     /* 004737d0 (cdecl) */
    void (*fade_in)(void *ctx, int32_t first, int32_t count, const uint8_t *rgb);  /* 00463e7c: eax, edx, ebx */
    int32_t (*ticks)(void *ctx);                                         /* 00473334 */
    void (*free)(void *ctx, int32_t bank);                               /* 00475260 (cdecl) */
    void *ctx;
} FrameBenchOps;

/* 004670e8: the screen cleared, title.qfs's palette ("!pal", palette_put
 * into a local one and the screen's) and background ("bgnd") drawn on the
 * screen picture, shown and faded in; then the same drawn and shown 100
 * times; returns 00473334's answer after them (the one before is dropped). */
int32_t frame_benchmark(int32_t screen, int32_t screen_shape, uint8_t screen_palette[PALETTE_PUT_BYTES],
                        const FrameBenchOps *ops);

#endif
