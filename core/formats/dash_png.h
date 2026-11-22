/* A dash mask's classes as the PNG the game reads (game/race/dash_mask.h's dash_mask_write, which the editor cannot link): an indexed PNG, class 0 black,
 * 64 blue, 128 green, 160 cyan, 192 red, 255 white. tests/test_dash_mask.c checks it is byte for byte what the game's own writer makes. */
#ifndef TNFS_DASH_PNG_H
#define TNFS_DASH_PNG_H

#include <stddef.h>
#include <stdint.h>

/* `classes` is width x height bytes of 0 / 64 / 128 / 160 / 192 / 255 (any other value is 0); 1 with `*out` malloc'd (free it) and `*len`, else 0. */
int dash_png_write(const uint8_t *classes, int width, int height, uint8_t **out, size_t *len);

#endif
