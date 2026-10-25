/* port: a movie on a wide screen (extras.cfg movies_wide). The exe draws a movie's picture twice the size in the 640 x 480
 * frame, which a wide screen shows boxed in the middle over its dimmed backdrop. Here the picture is scaled to the
 * screen's width and cropped at the top and bottom to its height (nearest: it is a palette picture). */
#ifndef TNFS_TGV_WIDE_H
#define TNFS_TGV_WIDE_H

#include <stdint.h>

/* `out`, width x height pixels, from a w x h picture (`pixels`, a row after the other): the picture scaled by
 * width / w each way, its middle rows kept when that is taller than `height`, black bars above and below when it is
 * shorter. Pixel (x, y) of the scaled picture takes the source pixel at its centre. */
void tgv_wide_scale(const uint8_t *pixels, int w, int h, uint8_t *out, int width, int height);

#endif
