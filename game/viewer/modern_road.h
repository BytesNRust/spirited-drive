/* port (spike): the near road without the PS1-era artefacts (modern_road.c). */
#ifndef TNFS_MODERN_ROAD_H
#define TNFS_MODERN_ROAD_H

#include "formats/fsh.h"
#include "game/gfx/quad.h"
#include "game/race/view.h"

#include <stdint.h>

/* A cell of `texture` (uv 16.16, corners in cam's order) from its four camera-space corners into q's clip box. */
void modern_road_cell(const QuadTarget *q, const View *view, const FshEntry *texture, const int32_t uv[8],
                      const int32_t *const cam[4], int transparent);

/* port: the 3D object engine's textured polygon (3 or 4 corners) the same way: uv 16.16 per corner, the mapper's
 * mode (a MAP_SHADE texel 0xfe takes shade[] of the pixel under it, `shade` 256 bytes then) */
void modern_polygon(const QuadTarget *q, const View *view, const FshEntry *texture, const int32_t uv[8],
                    const int32_t *const cam[4], int corners, MapMode mode, const uint8_t *shade);

/* port: a flat polygon (3 or 4 corners) of one palette colour, clipped the same way */
void modern_flat(const QuadTarget *q, const View *view, uint8_t colour, const int32_t *const cam[4], int corners);

#endif
