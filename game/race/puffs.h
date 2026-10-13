/* The tyre smoke's puffs (00456948, 00456820): the additive blend their
 * sprites draw through. */
#ifndef TNFS_PUFFS_H
#define TNFS_PUFFS_H

#include <stdint.h>

enum { PUFF_BLEND_SIZE = 256 * 256 };

/* 00456820 (watcall (table, palette)): palette 256 x 4 bytes (three channels
 * 0..255 then a spare). table[a x 256 + b] = nearest(min(pal[b] + pal[a] -
 * pal[a] / 4, 255) per channel), the channels packed c0 | c1 << 8 | c2 << 16
 * (00473140). Row a is the sprite's texel, column b what lies under it. */
void puff_blend_build(uint8_t table[PUFF_BLEND_SIZE], const uint8_t palette[1024],
                      uint8_t (*nearest)(void *ctx, uint32_t rgb), void *ctx);

#endif
