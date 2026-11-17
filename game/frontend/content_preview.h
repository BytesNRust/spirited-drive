/* port (dep59 step 4b, T7): the CONTENT screen's preview picture reduced to its box (content_screen.h's preview op), with no drawing and
 * no files: each pixel of the smaller picture is the mean of the source pixels it covers, a source pixel that is only partly under it
 * counted by the part that is (area averaging, so a 309 px card shrunk to 157 px keeps its thin lines as a shade instead of dropping
 * them, as the nearest pixel would). */
#ifndef TNFS_CONTENT_PREVIEW_H
#define TNFS_CONTENT_PREVIEW_H

#include <stdint.h>

/* `rgb` (w x h, 3 bytes a pixel, rows top down) into `out_rgb` (ow x oh), each channel the area-weighted mean rounded to the nearest
 * value; nothing written when a pointer is NULL or a size is not above 0 */
void content_reduce_area(const uint8_t *rgb, int w, int h, uint8_t *out_rgb, int ow, int oh);

#endif
