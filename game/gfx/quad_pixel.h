/* Writing one texel to the surface for each MapMode; shared by the quad and
 * triangle mappers (the span routines 004a070c, 004a02d8 and 004ac280, the row
 * blitters 0048cc49, 0048c89a and 0048ca53). */
#ifndef TNFS_GFX_QUAD_PIXEL_H
#define TNFS_GFX_QUAD_PIXEL_H

#include "game/gfx/quad.h"

enum { MAP_SKIP_TEXEL = 0xFF, MAP_SHADE_TEXEL = 0xFE };

typedef struct {
    MapMode mode;
    const uint8_t *shade;  /* 256 bytes, MAP_SHADE only */
} Paint;

static inline void paint_texel(const Paint *p, uint8_t *dst, uint8_t texel)
{
    switch (p->mode) {
    case MAP_OPAQUE: *dst = texel; break;
    case MAP_TRANSPARENT:
        if (texel != MAP_SKIP_TEXEL) *dst = texel;
        break;
    case MAP_SHADE:
        if (texel < MAP_SHADE_TEXEL) *dst = texel;
        else if (texel == MAP_SHADE_TEXEL) *dst = p->shade[*dst];
        break;
    case MAP_REMAP: *dst = p->shade[texel]; break;
    case MAP_REMAP_TRANSPARENT:
        if (texel != MAP_SKIP_TEXEL) *dst = p->shade[texel];
        break;
    }
}

static inline int paint_valid(const Paint *p)
{
    return (p->mode == MAP_OPAQUE || p->mode == MAP_TRANSPARENT) ||
           ((p->mode == MAP_SHADE || p->mode == MAP_REMAP || p->mode == MAP_REMAP_TRANSPARENT) && p->shade != NULL);
}

#endif
