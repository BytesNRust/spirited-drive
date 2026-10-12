/* The draw surface's clip rectangle (004c78bc..004c78c8) as the race's 2D
 * code sets it, kept inside the surface (004c78b4 x 004c78b8). */
#ifndef TNFS_CLIP_H
#define TNFS_CLIP_H

#include "game/gfx/quad.h"

#include <stdint.h>

/* 0047383c: left and top at least 0, right and bottom at most the
 * surface's width and height; nothing else is checked. */
void clip_set(QuadTarget *t, int32_t left, int32_t right, int32_t top, int32_t bottom, int32_t surface_w,
              int32_t surface_h);

#endif
