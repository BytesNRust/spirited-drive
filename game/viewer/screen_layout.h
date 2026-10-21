/* The wide screen's calculations: widths, views, fields of view and where the HUD's pieces go. Port glue, pure. */
#ifndef TNFS_SCREEN_LAYOUT_H
#define TNFS_SCREEN_LAYOUT_H

#include "game/race/race_view.h"
#include "game/sys/port_settings.h"

#include <stdint.h>

enum { SCREEN_HEIGHT = 480, ORIGINAL_WIDTH = 640, HUD_BAR_HEIGHT = 32 };

int screen_width(ScreenFormat f);                       /* 640, 854, 1120 */
int screen_menu_left(int width);                        /* (width - 640) / 2 */

/* The horizontal field that keeps the horizontal focal length of a 640 wide viewport with fov_x degrees in one of
 * `width` pixels; rounded to the nearest degree. */
int screen_fov_x(int fov_x_640, int width);
/* The same vertically for a viewport that goes from height_orig to height lines, plus fov_adjust degrees. */
int screen_fov_y(int fov_y_orig, int height_orig, int height, int fov_adjust);
/* The near plane's factor (16.16) for a vertical field widened from fov_y_exe to fov_y: the focal lengths' ratio, so
 * the road reaches the view's bottom as it did; 1 (65536) when the field is not wider. */
int32_t screen_near_scale(int fov_y_exe, int fov_y);

/* The 17 rows of the high mode (race_view_layouts[1]) for `width`: row 0 (cockpit) is width x 480, row 3 (outside,
 * 640x384 at y 32) is width wide with its clip box 448 lines (no strip, in 640 too; the projection keeps its 384); the 640-based rows (1, 2, 4..11) move right by
 * screen_menu_left(width); the mirror's row 12 and the unused rows stay as they are. */
void screen_layouts(int width, ViewLayout out[VIEW_LAYOUTS]);

/* The cockpit art's scale (k = width / 640, 16.16) and the lines of the scaled 640x480 art cut at the top. */
typedef struct { int32_t scale; int crop_top; } CockpitScale;
CockpitScale screen_cockpit_scale(int width);

/* Row 0 (inside the car) on a wide screen: the 640 view enlarged by screen_cockpit_scale with the dash and cropped
 * as it (the projection width x 480 k at -crop_top, the field and the vertical scale the same: the focal length
 * grows with the width on both axes), clipped to the windshield's 320 lines enlarged; the 640 row as it is at 640. */
ViewLayout screen_cockpit_view(int width);

typedef struct { int x, y; } ScreenPoint;
typedef struct {
    ScreenPoint bar_start, bar_middle, bar_end;   /* the scoreboard's three groups */
    ScreenPoint mirror, map, radar, gauge;
} HudAnchors;

/* `sizes`: width and height (x, y) of each piece, in HudAnchors order, as drawn (the mirror already scaled by k).
 * `mirror_640`: where the car's dash art puts the mirror in 640 (it differs per car: dash_points[0]). In 640 every
 * anchor is the piece's original position. */
HudAnchors screen_hud_anchors(int width, const ScreenPoint sizes[7], ScreenPoint mirror_640);

/* The format nearest to a desktop's proportions. */
ScreenFormat screen_format_nearest(int desktop_w, int desktop_h);

#endif
