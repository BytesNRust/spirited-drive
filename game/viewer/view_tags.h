/* the name tags over the cars (view_tags.c); view_cars.c draws them */
#ifndef TNFS_VIEW_TAGS_H
#define TNFS_VIEW_TAGS_H

#include "game/gfx/quad.h"

#include <stdint.h>

void view_tag_draw(const QuadTarget *target, int32_t x, int32_t y, int32_t slot, int32_t z);

#endif
