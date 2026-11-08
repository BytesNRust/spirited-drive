/* port (water_mist): the rain's spray as a soft, low, wide veil: a generated sprite with a smooth edge (no texel of the old speckle, no sprite of the CD's smoke), made in levels of a table each, thinning
 * to nothing with the particle's life. Pure: the shape and the numbers, no drawing. Variants: 1 a veil, 2 the veil and a few fine droplet streaks by the tyre, 3 a cooler, flatter, wider and
 * thinner veil with the streaks. */
#ifndef TNFS_WATER_MIST_H
#define TNFS_WATER_MIST_H

#include <stddef.h>
#include <stdint.h>

enum { WATER_MIST_W = 64, WATER_MIST_H = 32, WATER_MIST_BYTES = 16 + WATER_MIST_W * WATER_MIST_H, WATER_MIST_LEVELS = 8, WATER_MIST_VARIANTS = 3, WATER_MIST_DENSITIES = 4 };

/* The shape (type 0x7b, width +4, height +6, texels from +0x10): each texel a level 0..WATER_MIST_LEVELS, the heart the highest, the border 0, falling smoothly. */
void water_mist_shape(uint8_t out[WATER_MIST_BYTES]);

/* How much of the veil's colour (0..255) the top level adds, for the variant 1..3 and the particle's density 0..WATER_MIST_DENSITIES - 1 (0 the thinnest, the end of its life). */
int water_mist_amount(int variant, int density);

/* The veil's colour for the variant. */
void water_mist_colour(int variant, uint8_t rgb[3]);

/* The veil's width and height in percent of the particle's pixel width: wider and flatter than a puff. */
int water_mist_width_pct(int variant);
int water_mist_height_pct(int variant);

/* Whether the variant throws fine droplets by the tyre. */
int water_mist_streaks(int variant);

#endif
