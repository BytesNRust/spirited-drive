/* port: the graphics screen's additions (not in the original): a fourth view distance, "ultra", and a second page for
 * the port's own settings (screen format, refresh rate, scaling, full screen, field of view). Drawn with the game's
 * text routine over the screen's own art; graphics_menu.c calls in only when its state has a `plus`. */
#ifndef TNFS_GRAPHICS_PLUS_H
#define TNFS_GRAPHICS_PLUS_H

#include "game/frontend/graphics_menu.h"

/* the second page's settings, in row order */
enum { PLUS_FORMAT, PLUS_REFRESH, PLUS_SCALE, PLUS_FULLSCREEN, PLUS_FOV, PLUS_SETTINGS };

/* the page indicator at the bottom centre ("1/2 >" / "< 2/2"): a click in it turns the page (pointer_in_rect's order) */
enum { PLUS_NAV_X0 = 270, PLUS_NAV_X1 = 370, PLUS_NAV_Y0 = 432, PLUS_NAV_Y1 = 476 };

/* a setting the next (direction 1) or the previous (-1) of its values, wrapping; turning full screen on takes
 * `fullscreen_format` (the desktop's) as the format, off leaves it */
void graphics_plus_step(PortSettings *s, int setting, int direction, ScreenFormat fullscreen_format);
void graphics_plus_value(const PortSettings *s, int setting, char out[16]);
/* the view distance's four values in a cycle: far (0), medium (1), close (2), ultra (far's art with `ultra` set) */
void graphics_plus_view_step(int16_t *view, int *ultra);

/* "Ultra" in place of the view value's art (drawn after it; lit as the selected item is) */
void graphics_plus_ultra(const GraphicsMenuState *st, const GraphicsMenuOps *o, const uint8_t *rgb, int lit);
/* the page indicator of page 1 (0) or 2 (1) */
void graphics_plus_nav(const GraphicsMenuState *st, const GraphicsMenuOps *o, int page);
int graphics_plus_nav_hit(const GraphicsMenuOps *o);

typedef enum { PLUS_PAGE_DONE, PLUS_PAGE_CANCEL, PLUS_PAGE_BACK } PlusPageResult;
/* The second page, run to its end over the screen's `bank` and `rgb` palette: Done or the idle deadline leave with
 * the settings as they are, Cancel and Escape say so (the caller puts them back), PageUp or a click on the indicator
 * go back to page 1. */
PlusPageResult graphics_plus_page(const GraphicsMenuState *st, const GraphicsMenuOps *o, int32_t bank,
                                  const uint8_t *rgb);

#endif
