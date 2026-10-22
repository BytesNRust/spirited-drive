/* port: EXTRAS' Texture Filter on the 3D sky ring (sky_ring.h, the circuits' sky). The exe maps the 'horz' bitmap
 * onto the ring's slices with its triangle mapper (00486400, quad.h's tri_draw), magnified texel by texel; here each
 * pixel of a triangle blends the four texels about its point of the bitmap as the flat horizon's filter does
 * (horizon_smooth.h: between texel centres, a 4x4 ordered dither, the palette's nearest colour). The pixels covered
 * are tri_draw's rule (pixel centres, a row's run from its left edge up to but not including its right) on exact
 * edges, so two triangles sharing an edge meet without a gap or an overlap. */
#ifndef TNFS_SKY_SMOOTH_H
#define TNFS_SKY_SMOOTH_H

#include "formats/fsh.h"
#include "game/gfx/quad.h"
#include "viewer/horizon_smooth.h"

#include <stdint.h>

/* tri_draw(target, texture, uv, screen, 0) blended: uv 3 x (u, v) in whole texels, screen 3 x (x, y) in whole pixels.
 * An invalid target or texture draws nothing. */
void sky_smooth_triangle(const HorizonSmooth *s, const QuadTarget *target, const FshEntry *texture, const int32_t uv[6],
                         const int32_t screen[6]);

#endif
