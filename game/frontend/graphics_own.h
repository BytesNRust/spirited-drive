/* port: the Graphics screen, the port's own: one page over the configuration screens' clean background with no burnt-in
 * text, the game's own letters and its Done/Cancel, the twelve settings in the order the owner named them. Time of day
 * and weather are not here: they are set on the track screen (port.cfg keeps psx_time and weather). Not in the original:
 * the original's graphics_menu.c (00423d28) is kept as it is, and no longer on any path of the game -- both Options
 * and the race's pause menu open this screen. In the race (GraphicsPlus.race) the rows the next race reads are shown
 * dimmed and cannot be changed (graphics_own_live_in_race).
 *
 * The rows edit the mode's settings; what the mode holds is dimmed, shows its value and does not change (port_profile.h). */
#ifndef TNFS_GRAPHICS_OWN_H
#define TNFS_GRAPHICS_OWN_H

#include "game/frontend/graphics_menu.h"

/* the twelve settings, in the owner's order, then the two buttons. DONE restarts from SETTINGS, as graphics_plus.h's
 * ROW_DONE does: without it the two buttons would start one row after the settings' count. A row's number is also its
 * place in the labels' table. LIGHT is "Light effects": psx_light and psx_shadow together. */
enum { GRAPHICS_OWN_PERSPECTIVE, GRAPHICS_OWN_FILTER, GRAPHICS_OWN_VIEW, GRAPHICS_OWN_FOG, GRAPHICS_OWN_LIGHT,
       GRAPHICS_OWN_FOV, GRAPHICS_OWN_FORMAT, GRAPHICS_OWN_REFRESH, GRAPHICS_OWN_FULLSCREEN, GRAPHICS_OWN_SCALING,
       GRAPHICS_OWN_BANDS, GRAPHICS_OWN_SCALE, GRAPHICS_OWN_SETTINGS, GRAPHICS_OWN_DONE = GRAPHICS_OWN_SETTINGS,
       GRAPHICS_OWN_CANCEL, GRAPHICS_OWN_ROWS };

/* the clean background's size, as config_bgnd.h's CONFIG_BGND_W and _H */
enum { GRAPHICS_OWN_W = 640, GRAPHICS_OWN_H = 480 };

/* where the labels sit: the left edge of the column, the values' right edge. The rows' y and the pointer's bands are
 * the file's own tables, from the exe's. */
enum { GRAPHICS_OWN_LABEL_X = 8, GRAPHICS_OWN_VALUE_RIGHT = 493 };

/* A setting the next (direction 1) or the previous (-1) of its values, wrapping; turning full screen on takes
 * `fullscreen_format` (the desktop's) as the format, off leaves it. The view distance is not one of these: it steps
 * through config.dat's own short with graphics_own_view_step, which is where the race reads it from. */
void graphics_own_step(PortSettings *s, int setting, int direction, ScreenFormat fullscreen_format);
void graphics_own_value(const PortSettings *s, int setting, char out[16]);
/* the view distance's four values: the exe's three names for its own, and "Ultra" for the port's fourth */
void graphics_own_view_step(int16_t *view, int *ultra);
void graphics_own_view_value(int16_t view, int ultra, char out[16]);
/* the race's pause menu: Far, Medium, Close and back to Far. Landing on Far drops the Ultra flag, so the row never
 * stops on a value the race cannot leave */
void graphics_own_race_view_step(int16_t *view, int *ultra);
/* whether the pause menu's Graphics may change `setting` (a GRAPHICS_OWN_ row) now: 0 for the rows the next race
 * reads (scaling, the format, full screen, the render threads, the render scale) and for Ultra, which is view 0 with
 * the flag. */
int graphics_own_live_in_race(int setting, int16_t view, int ultra);
int graphics_own_view_live(int f3_ultra, int16_t view, int ultra);
void graphics_own_race_view_step_for(int f3_ultra, int16_t *view, int *ultra);

/* The screen, run over the same ops as graphics_menu.c (00423d28) and behaving like it: the arrows move, Space / Enter
 * / the left button choose, Done saves through `plus_save`, Escape and Cancel put the settings back with
 * `plus_put_back` and save nothing. The idle deadline and the right button leave the way Cancel does. Returns 1 when
 * Done saved, 0 when the settings were put back.
 *
 * The background and the title go through three port slots, all optional: `plus_bgnd` composes the clean background
 * into the screen's own buffers once (nothing else is drawn over the caller's picture when it is absent),
 * `plus_bgnd_shape` puts it on the screen and its palette with it, `plus_title` draws the title when the glue wants
 * its own (otherwise this file's own is drawn). */
int32_t graphics_own(const GraphicsMenuState *st, const GraphicsMenuOps *ops);

#endif
