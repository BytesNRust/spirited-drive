/* A wide line darkened through the shade remap (00491fa0; the skid marks'
 * strokes, from 00459eb8 and 00459e1c). */
#ifndef TNFS_THICK_LINE_H
#define TNFS_THICK_LINE_H

#include "game/gfx/quad.h"

#include <stdint.h>

/* 00491fa0 (cdecl x0, y0, x1, y1, width): the line's angle (00484294 of dx
 * and dy x 5 / 4), the width across it (-cos, and sin cut to 13 / 16 down),
 * a quad half each side of both ends shaded by remap (00482794). */
void thick_line_shade(const QuadTarget *target, const uint8_t *remap, int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                      int32_t width);

#endif
