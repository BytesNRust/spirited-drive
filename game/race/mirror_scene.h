/* The rear-view mirror's scene (0044baa4 and the pieces it calls): the
 * segments behind the camera drawn into the mirror's picture. */
#ifndef TNFS_MIRROR_SCENE_H
#define TNFS_MIRROR_SCENE_H

#include "game/race/race_view.h"

#include <stdint.h>

/* The camera's screen box (00511f88..00511f94). */
typedef struct {
    int32_t bottom;             /* 00511f88: top + the layout's +0x24 */
    int32_t right;              /* 00511f8c: left + the layout's +0x20 */
    int32_t left, top;          /* 00511f90, 00511f94: the layout's +0x18, +0x1c */
} MirrorBounds;

/* 00449f50: the box of the camera's layout (*(00511fe8) +0x1c). */
void mirror_bounds(const ViewLayout *layout, MirrorBounds *b);

/* 0044a154: 0 when the quad's four x (screen[0, 2, 4, 6]) are all left of
 * the box or all right of it, or its points 1 and 2 are both below it
 * (screen[3], screen[5] past bottom); else 1. */
int mirror_quad_visible(const MirrorBounds *b, const int32_t screen[8]);

enum { MIRROR_SEGMENT_RECORD = 0x21c, MIRROR_SEGMENT_CACHE = 0x8700 };

/* 0043e95c: the byte offset of a segment's record in the segment cache
 * (005041d8): (segment & 00504210) x 0x21c (32-bit) mod 0x8700, unsigned. */
uint32_t mirror_segment_offset(int32_t segment, int32_t mask);

#endif
