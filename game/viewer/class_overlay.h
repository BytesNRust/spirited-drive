/* port (frame budget): the 2D layer's marks laid over the light's world mask (drive_light_world_mask), a row of the layer at a time.
 * Most of the layer is "nothing drawn" (the 3D shows), so a layer row without a mark is found once, when the layer is taken, and its
 * output rows are not visited; a row with marks writes `scale` bytes a mark, with no division. The mask comes out byte for byte as the
 * per-pixel loop made it (tests/test_class_overlay.c holds that loop). */
#ifndef TNFS_CLASS_OVERLAY_H
#define TNFS_CLASS_OVERLAY_H

#include <stddef.h>
#include <stdint.h>

/* marks[row] = 1 when the layer's row holds a class that changes the mask (PSX_CLASS_DASH .. PSX_CLASS_LIT_COLOUR + PSX_CLASS_LIT_COLOURS - 1), else 0 */
void class_overlay_rows(const uint8_t *layer, int width, int height, uint8_t *marks);

/* The marks over `mask` (mask_width x rows bytes, mask_width = width * scale): DASH 2, HUD 0, LIT and the colours as they are; any other class leaves the byte.
 * Rows of the output past the layer's height * scale are left alone. */
void class_overlay_apply(uint8_t *mask, int mask_width, size_t rows, const uint8_t *layer, int width, int height, int scale, const uint8_t *marks);

#endif
