/* port (spray): the particles the cars throw up, taken out of ported_view.c: rain_fx's spray and the water mist, the rally's dust, the CD's smoke sprites, spray v4's glue. ported_view.c draws a road row through
 * view_spray_row and sets the frame up through view_spray_frame; what view_spray.c needs of the view (its target, the race, the camera) is declared below and defined in ported_view.c. */
#ifndef TNFS_VIEW_SPRAY_H
#define TNFS_VIEW_SPRAY_H

#include <stdint.h>

#include "game/race/car_struct.h"
#include "game/race/model_draw.h"
#include "game/race/puffs.h"
#include "game/race/race_state.h"
#include "game/race/track_art.h"
#include "viewer/object_sink.h"
#include "viewer/view_rain.h"
#include "viewer/view_shared.h"
#include "viewer/weather_sky.h"

enum { SPRAY_DIV = 0x14000, VIEW_WORLD_SHIFT = 11 };   /* SPRAY_DIV: a spray puff's pixel radius is depth x size / SPRAY_DIV; VIEW_WORLD_SHIFT is ported_view.c's WORLD_SHIFT (004c52bc) */

/* ---- ported_view.c calls ---- */
void view_spray_frame(const TrackArt *art);
void view_spray_v4_hook(void);   /* spray v4's emitter on the sim's ticks now, not at the first frame drawn (the headless shot ticks before it draws) */
void view_spray_row(int32_t node, int far_side);   /* far_side 1: before the row's cars; 0: after them */
void view_spray_dust_draw(int slot);
void view_spray_trail_puff(void *ctx, int32_t scale, int32_t x, int32_t y);

#endif
