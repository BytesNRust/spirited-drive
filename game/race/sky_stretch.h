/* port (sky_fill): the flat sky's art extended upward until it reaches the top of the view.
 * The exe draws the art (horizon.h: horizon_flat) with its horizon row on the horizon line and fills what lies above
 * the art's top with the art's first pixel; on a wide, tall view the line sits lower than the art's rows above it
 * reach, and the dawn and dusk art, whose top row is already cloud, ends in a hard edge against that flat colour.
 * The art is not stretched, so it does not move with the camera: rows are added on top of it, in one of two ways. */
#ifndef TNFS_SKY_STRETCH_H
#define TNFS_SKY_STRETCH_H

#include "formats/fsh.h"

#include <stddef.h>
#include <stdint.h>

/* how the added rows are filled */
enum {
    SKY_FILL_COLUMN = 1,   /* every column takes the most frequent colour of its top rows (SKY_FILL_COLUMN_ROWS) */
    SKY_FILL_COLUMN_QUIET = 3,   /* as SKY_FILL_COLUMN, but a star (a run of up to SKY_FILL_STAR_WIDTH columns lighter than the pixel under it by SKY_FILL_STAR_LUMA and 3 times as light (SKY_FILL_STAR_RATIO_X2 / 2)) is not carried up: the columns beside it are */
    SKY_FILL_BLEND = 2     /* one colour (the nearest palette entry to the mean of the top rows), dithered into the top row */
};
enum { SKY_FILL_STAR_LUMA = 8, SKY_FILL_STAR_RATIO_X2 = 6, SKY_FILL_STAR_WIDTH = 6 };   /* how much lighter than the pixel under it a top pixel must be to be a star (palette lightness, 0..255) */
enum { SKY_FILL_MEAN_ROWS = 4, SKY_FILL_BLEND_ROWS = 12, SKY_FILL_COLUMN_ROWS = 8 };

typedef struct {
    FshEntry entry;
    uint8_t *pixels;
    size_t cap;
} SkyStretch;   /* zero before the first call */

/* `horz` as horizon_flat draws it on screen row `line` with `scale` (16.16 screen rows per bitmap row) in a box whose
 * top is `top`: the same entry when its rows above the horizon (misc[1]) already reach `top` (or `mode` is unknown, or
 * it has nothing to extend), else `s`'s entry, whose misc[1] is the fewest rows that do. The art's own rows are copied
 * as they are. `palette` is 3 bytes per index (SKY_FILL_BLEND and SKY_FILL_COLUMN_QUIET only). The result stays valid until the next call. */
const FshEntry *sky_stretch(SkyStretch *s, const FshEntry *horz, int32_t scale, int32_t line, int32_t top, int mode,
                            const uint8_t *palette);

/* port (sky_fill_below): the same downward. horizon_flat fills what lies under the art with the art's last pixel, one flat colour; at night against the black
 * of the frame it is a hard edge. This adds rows under the art, enough to reach `bottom` (the box's bottom row, screen rows), every column taking the most frequent
 * colour of its last SKY_FILL_BELOW_ROWS rows (a lone pixel, a star, is outvoted; a tie goes to the lower row). The same entry when the art already reaches
 * `bottom` (or there is nothing to extend); never more than HORIZON_ROWS output rows in all, the horizon table's reach. Valid until the next call. */
enum { SKY_FILL_BELOW_ROWS = 8 };
const FshEntry *sky_stretch_below(SkyStretch *s, const FshEntry *horz, int32_t scale, int32_t line, int32_t bottom);

/* the colour column x of `horz` (w x h) takes under the art: the most frequent of its last `rows` rows, 0xff (transparent) only when all are */
uint8_t sky_below_colour(const uint8_t *payload, int32_t w, int32_t h, int32_t x, int32_t rows);

void sky_stretch_free(SkyStretch *s);

#endif
