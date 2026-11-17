/* port (dep59, Lunar Springs' entry, L4): what lunar_art.c and lunar_art_title.c share. Not for other files. */
#ifndef TNFS_LUNAR_ART_PRIVATE_H
#define TNFS_LUNAR_ART_PRIVATE_H

#include "game/frontend/lunar_art.h"

#include <stddef.h>
#include <stdint.h>

enum { GLYPH_MAX_W = 64 };

/* a letter of the title as its difference from the texture under it (its white and its shadow), w x h from (its run's left - 3, the
 * title's first row); `width` its run's, for the next letter's place */
typedef struct {
    int16_t delta[GLYPH_MAX_W * (LUNAR_TITLE_Y1 - LUNAR_TITLE_Y0)];
    int w, h, width;
} Glyph;

/* the reason, printf-like, into `why` */
void lunar_why(char *why, size_t why_size, const char *format, ...);

/* the grey picture `g` (640 x 480) with LUNAR for RUSTY, its L from the grey `lost` (Lost Vegas' picture); 0 with the reason */
int lunar_title(uint8_t *g, const uint8_t *lost, char *why, size_t why_size);

/* the time of day's line (the baked label and its value) covered by the rows under it */
void lunar_drop_time_line(uint8_t *g);

#endif
