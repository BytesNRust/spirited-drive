/* port (wipers): the geometry of the wiper blades on a dash, measured from the dash's art (and the mask's blade colour as glass): the pure part of drive_wide's wiper scene, so it can be tested on art made for the
 * test. Pixels are the dash layer's (640x480, y down). */
#ifndef TNFS_WIPER_SCENE_H
#define TNFS_WIPER_SCENE_H

#include "game/race/wiper.h"

#include <stdint.h>

/* The windscreen's glass alone as flags (1 = glass, 640x480): the art's glass (LAYER_KEY, or what the mask marks as a blade) connected by 4 neighbours to the first glass found going down from the top of the
 * view at the middle columns. `flags` is SCREEN_W * SCREEN_H bytes, `stack` SCREEN_W * SCREEN_H ints of work area. */
void wiper_scene_glass_flags(const uint8_t *art, const uint8_t *mask, uint8_t *flags, int *stack);

/* The glass's lowest row in column x, less 2 (inside it); -1 when the column has none. */
float wiper_scene_windscreen_bottom(const uint8_t *flags, int x);

/* The glass's extent along a row: its first and last flagged columns. */
void wiper_scene_glass_extent(const uint8_t *flags, int y, int *left, int *right);

/* The lowest row with a wide stretch of glass, 10 rows up from it (the middle of the view when none). */
int wiper_scene_glass_row(const uint8_t *flags);

/* How far (degrees, at most max_span) a blade turns from its park and stays on the glass. */
float wiper_scene_blade_fit(const uint8_t *art, const uint8_t *mask, const WiperBlade *b, float max_span);

/* The x of the steering wheel's left rim read from the glass's lower boundary; `fallback` when there is no bump. */
float wiper_scene_wheel_left_rim(const uint8_t *flags, float ya, float tn, int xa, float fallback);

/* The pair of blades on the layer and how far they sweep (degrees), from the art, the mask and the flags of wiper_scene_glass_flags. */
void wiper_scene_measure(const uint8_t *art, const uint8_t *mask, const uint8_t *flags, WiperBlade layer[WIPER_BLADES], float *span);

#endif
