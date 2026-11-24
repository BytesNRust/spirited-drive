/* The strip of a light line (lamp, brightness, tone): which of a row's 11 points it takes (formats of notes/track-light-format-2026-11-22: point 0 the centre, 1..5 chain
 * to the right, 6..10 chain to the left). A strip is a mask, bit k = point k. In a file it is a word (right = p1..p5, left = p6..p10, road = p0 p1 p6, all = p0..p10) or a
 * range of points `pA pB` (`pA` alone: one point) or a list `p1,p3,p7` (any points). The panel's bar draws the row as the road is seen: left outer to right outer, the centre in the middle. */
#ifndef TNFS_EDITOR_LIGHT_STRIP_H
#define TNFS_EDITOR_LIGHT_STRIP_H

#include <stddef.h>

enum {
    LIGHT_STRIP_POINTS = 11,
    LIGHT_STRIP_ALL = 0x7ff,
    LIGHT_STRIP_RIGHT = 0x03e,
    LIGHT_STRIP_LEFT = 0x7c0,
    LIGHT_STRIP_ROAD = 0x043
};

#ifdef __cplusplus
extern "C" {
#endif

/* The strip written in words[0..n): a shortcut word (1 word), `pA pB` (2 words), `pA` (1 word) or a list `p1,p3,p7` (1 word). Returns the words it used (0: not a strip, the mask is left alone) and
 * sets *mask. A point range with A > B or a point above 10 is not a strip. */
int light_strip_parse(const char *const *words, int n, unsigned *mask);

/* The mask as the words a file takes: a shortcut when it is one, else `pA pB` / `pA` when its points are consecutive, else the list `p1,p3,p7`. Returns 1 and fills `out` (48 bytes hold any); 0 when the mask is empty
 * and `out` is "". */
int light_strip_text(unsigned mask, char *out, size_t cap);

/* The bar's cells, from the road's left to its right: the point each shows (10 9 8 7 6, 0, 1 2 3 4 5). */
int light_strip_cell_point(int cell);


#ifdef __cplusplus
}
#endif

#endif
