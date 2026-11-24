/* port (track light, STRIP): the strip of a light line as a list of points in one word, `p1,p3,p7` (the points of a row, p0..p10, in any order, each once or more): the
 * words `left` / `right` / `road` / `all` and the range `pA pB` stay as they were. The game's parser and Studio's both read and write it through here. */
#ifndef TNFS_CORE_FORMATS_LIGHT_POINTS_H
#define TNFS_CORE_FORMATS_LIGHT_POINTS_H

#include <stddef.h>

/* `word` as a list of points: 1 with the points in *mask (bit p); 0 when the word has no comma (not a list, *mask untouched); -1 for a comma word that is no list (a point above 10,
 * an empty item, a letter, no leading p) */
int light_points_parse(const char *word, unsigned *mask);

/* `mask` (bits 0..10, not empty) as `p1,p3,p7` into `out`; 0 when the mask is empty or has a bit above 10 (`out` is then "") */
int light_points_text(unsigned mask, char *out, size_t cap);

#endif
