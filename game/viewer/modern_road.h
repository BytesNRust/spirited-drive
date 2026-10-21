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

#endif
