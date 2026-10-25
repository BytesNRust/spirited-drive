/* port: EXTRAS' Texture Filter on the flat horizon (horizon.h's 0043ae64 / 0043a18c). The exe draws the 'horz'
 * bitmap magnified texel by texel (2 to 3.5 screen pixels a texel for the 640 x ~120 skies of Alpine, Coastal and
 * City); here each screen pixel blends the four texels about it by where it falls between their centres, the blend
 * the palette's nearest colour (a 15-bit table), a texel's own colour where the four are one. The panorama wraps on
 * its own row (the exe reads the row beside it at the seam). The fills above and below are the exe's. */
#ifndef TNFS_HORIZON_SMOOTH_H
#define TNFS_HORIZON_SMOOTH_H

#include "game/race/horizon.h"

#include <stdint.h>

enum { HORIZON_SMOOTH_CELLS = 1 << 15 };

typedef struct {
    uint8_t palette[768];                       /* the one `nearest` was made for */
    uint8_t nearest[HORIZON_SMOOTH_CELLS];      /* r >> 3 << 10 | g >> 3 << 5 | b >> 3: the nearest colour */
    int ready;
} HorizonSmooth;

/* The table for `palette` (768 bytes, 0..255 a channel), remade only when it changed; 0xfe and 0xff, the mappers'
 * markers, never chosen. */
void horizon_smooth_palette(HorizonSmooth *s, const uint8_t palette[768]);

/* The table's colour for r, g, b. */
uint8_t horizon_smooth_nearest(const HorizonSmooth *s, int r, int g, int b);

/* port: one pixel of `bitmap` (8-bit, width x height texels) blended at (u, v), 16.16 texel positions with texel k's
 * centre at k (the four texels about it, clamped at the bitmap's edges), dithered for screen x, y; the texel itself
 * where the four are one (the 3D sky ring's, sky_smooth.h). */
uint8_t horizon_smooth_texel(const HorizonSmooth *s, const FshEntry *bitmap, int32_t u, int32_t v, int x, int y);

/* port: the pixels x0..x1 of `row` (screen row y) as horizon_smooth_texel gives them, the texel position of pixel x
 * floor(base + dx * x + dy * y) for u = { base, dx, dy } and v likewise. */
void horizon_smooth_span(const HorizonSmooth *s, const FshEntry *bitmap, uint8_t *row, int32_t x0, int32_t x1, int y,
                         const double u[3], const double v[3]);

/* horizon_draw_flat with the band blended. */
void horizon_draw_flat_smooth(const HorizonSmooth *s, const HorizonTables *t, const HorizonTarget *target,
                              const ClipRect *clip, const FshEntry *horz, int32_t yaw, int mirrored, int sky_off,
                              int32_t line);

/* port: the band cache's frames (horizon_draw_flat_smooth keeps each render band's pixels and copies them when the same
 * draw comes again): free it, and count the copies made (tests; the cache is the same pixels either way). */
void horizon_smooth_cache_reset(void);
void horizon_smooth_cache_enable(int on);   /* 1 (the default) keeps and copies, 0 draws every time and leaves the cache as it was */
unsigned long horizon_smooth_cache_hits(void);

#endif
