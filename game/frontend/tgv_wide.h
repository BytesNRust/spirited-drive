/* port: a movie on a wide screen (extras.cfg movies_wide). The exe draws a movie's picture twice the size in the 640 x 480
 * frame, which a wide screen shows boxed in the middle over its dimmed backdrop. Here the picture is scaled to the
 * screen's width and cropped at the top and bottom to its height (nearest: it is a palette picture), as long as the
 * crop is small (16:9: 32 rows of 480, 6.7 %). Where it would cut more than TGV_WIDE_MAX_CROP_PERCENT of the height
 * (21:9: 40 %) the picture is fitted to the height instead, nothing cropped, and the sides show the same frame
 * enlarged to cover them and dimmed like the menus' backdrop (backdrop.h). */
#ifndef TNFS_TGV_WIDE_H
#define TNFS_TGV_WIDE_H

#include <stdint.h>

/* `out`, width x height pixels, from a w x h picture (`pixels`, a row after the other): the picture scaled by
 * width / w each way, its middle rows kept when that is taller than `height`, black bars above and below when it is
 * shorter. Pixel (x, y) of the scaled picture takes the source pixel at its centre. */
void tgv_wide_scale(const uint8_t *pixels, int w, int h, uint8_t *out, int width, int height);


enum { TGV_WIDE_MAX_CROP_PERCENT = 10 };

/* 1 when the picture fills the width (the crop at the screen's width, if any, is at most TGV_WIDE_MAX_CROP_PERCENT of
 * `height`); 0 when it is fitted to the height with dimmed sides */
int tgv_wide_fills(int w, int h, int width, int height);

/* `out` as tgv_wide_scale when the picture fills the width; else the picture scaled to `height` (w * height / h wide,
 * centred, nearest at pixel centres), the rest of each row the picture enlarged to cover the width (cut equally at the
 * top and bottom) with each colour replaced by shade[colour] (backdrop_shade_table). */
void tgv_wide_compose(const uint8_t *pixels, int w, int h, uint8_t *out, int width, int height, const uint8_t shade[256]);

#endif
