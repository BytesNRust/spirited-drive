/* port (wipers): the geometry of the wiper blades on a dash, measured from the dash's art (and the mask's blade colour as glass): the pure part of drive_wide's wiper scene, so it can be tested on art made for the
 * test. The flags are the dash layer's pixels (640x480, y down); the measure is on the glass as the frame shows it (wiper_scene_frame_glass). */
#ifndef TNFS_WIPER_SCENE_H
#define TNFS_WIPER_SCENE_H

#include "game/race/wiper.h"

#include <stdint.h>

/* The windscreen's glass alone as flags (1 = glass, 640x480): the art's glass (LAYER_KEY, or what the mask marks as a blade) connected by 4 neighbours to the first glass found going down from the top of the
 * view at the middle columns. `flags` is SCREEN_W * SCREEN_H bytes, `stack` SCREEN_W * SCREEN_H ints of work area. */
void wiper_scene_glass_flags(const uint8_t *art, const uint8_t *mask, uint8_t *flags, int *stack);

/* The shares of the glass the frame shows (its width at the pair's row, from the left pillar to the frame's right edge) where the pivots are and how long the blades are: the owner's sketch, measured on the
 * P911 at 21:9, so the pair keeps its place on the windscreen at any width: the driver's pivot a quarter of the way in, the other near two thirds. The blades' length is a share of the frame's height: the dash is drawn at the same size at every width (its
 * middle 1:1), so the blades are too, and a 4:3 view crops the right blade at the screen's edge as it crops the glass. */
#define WIPER_LEFT_SHARE 0.243f
#define WIPER_RIGHT_SHARE 0.634f
#define WIPER_LEN_SHARE 0.535f
/* The pivots' distance either side of the steering wheel's middle, a share of the frame's height (the dash is drawn 1:1 at every width): the owner's marks on the CZR1's glass, 2026-11-12, about 127 of the
 * dash's pixels each way. The shares of the glass above place the pair on a dash with no wheel. */
#define WIPER_PIVOT_SHARE 0.265f

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

/* The steering wheel's middle column on the 640 dash: the middle of the columns its frames reach in any pose (`zone`, wheel_front_zone's 640x480 flags); -1 when it reaches none. */
float wiper_scene_wheel_x(const uint8_t *zone);

/* The column of a frame `width` wide that shows the dash's column `dash_x` (of the dash's 1:1 middle, screen_dash_x): dash_x itself at 640. */
float wiper_scene_frame_x(int width, float dash_x);

/* The pair of blades on a glass map `width` wide (its pixels) and how far they sweep (degrees): the pivots WIPER_PIVOT_SHARE of the frame's height either side of `wheel_x` (the wheel's middle on the
 * frame, wiper_scene_frame_x), or at the glass's shares when it is negative (no wheel). */
void wiper_scene_measure(const uint8_t *map, int width, float wheel_x, WiperBlade blades[WIPER_BLADES], float *span);

#endif
