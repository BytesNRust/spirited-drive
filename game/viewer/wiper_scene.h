/* port (wipers): the geometry of the wiper blades on a dash, measured from the dash's art (and the mask's blade colour as glass): the pure part of drive_wide's wiper scene, so it can be tested on art made for the
 * test. The flags are the dash layer's pixels (640x480, y down); the measure is on the glass as the frame shows it (wiper_scene_frame_glass). */
#ifndef TNFS_WIPER_SCENE_H
#define TNFS_WIPER_SCENE_H

#include "game/race/wiper.h"

#include <stdint.h>

/* The windscreen's glass alone as flags (1 = glass, 640x480): the art's glass (LAYER_KEY, or what the mask marks as a blade) connected by 4 neighbours to the first glass found going down from the top of the
 * view at the middle columns. `flags` is SCREEN_W * SCREEN_H bytes, `stack` SCREEN_W * SCREEN_H ints of work area. */
void wiper_scene_glass_flags(const uint8_t *art, const uint8_t *mask, uint8_t *flags, int *stack);

/* The shares of the glass the frame shows (its width at the pair's row, from the left pillar to the frame's right edge) where the pivots are and how long the blades are: the owner's (i197), measured on the
 * P911 at 21:9, so the pair keeps its place on the windscreen at any width: the driver's pivot a quarter of the way in, the other near two thirds, each blade reaching about 0.29 of the glass at rest. */
#define WIPER_LEFT_SHARE 0.243f
#define WIPER_RIGHT_SHARE 0.634f
#define WIPER_LEN_SHARE 0.29f

/* The glass as the frame shows it, `width` columns by 480 rows into `map`: at 640 the layer's flags; wider, each pixel the layer's pixel the dash is drawn from there (screen_dash_x / screen_dash_y: its middle
 * as it is, its sides stretched, dropped by DASH_DROP), so the blades are placed and drawn on the glass the player sees. */
void wiper_scene_frame_glass(const uint8_t *flags, int width, uint8_t *map);

/* On a glass map `width` wide (480 rows). The glass's lowest row in column x, less 2 (inside it); -1 when the column has none. */
float wiper_scene_windscreen_bottom(const uint8_t *map, int width, int x);

/* The glass's extent along a row: its first and last flagged columns. */
void wiper_scene_glass_extent(const uint8_t *map, int width, int y, int *left, int *right);

/* The lowest row with a wide stretch of glass, 10 rows up from it (the middle of the view when none). */
int wiper_scene_glass_row(const uint8_t *map, int width);

/* How far (degrees, at most max_span) a blade turns from its park and stays on the glass. */
float wiper_scene_blade_fit(const uint8_t *map, int width, const WiperBlade *b, float max_span);

/* The pair of blades on a glass map `width` wide (its pixels) and how far they sweep (degrees). */
void wiper_scene_measure(const uint8_t *map, int width, WiperBlade blades[WIPER_BLADES], float *span);

#endif
