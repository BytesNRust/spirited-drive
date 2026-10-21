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

/* horizon_draw_flat with the band blended. */
void horizon_draw_flat_smooth(const HorizonSmooth *s, const HorizonTables *t, const HorizonTarget *target,
                              const ClipRect *clip, const FshEntry *horz, int32_t yaw, int mirrored, int sky_off,
                              int32_t line);

#endif
