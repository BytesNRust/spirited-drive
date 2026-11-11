/* the tunnel lamps' textures, tables and halos (view_lamps.c); ported_view.c calls these. ported_view_set_tunnel_lamps is defined there too (ported_view.h) */
#ifndef TNFS_VIEW_LAMPS_H
#define TNFS_VIEW_LAMPS_H

#include "formats/fsh.h"
#include "game/gfx/quad.h"
#include "game/race/tunnel_lamp_textures.h"
#include "game/race/track_art.h"
#include "game/viewer/modern_road.h"

#include <stdint.h>

void view_lamps_set_flares(int on);     /* lamp_flares: the halos round the lit lamps and brake lights */
int view_lamps_flares(void);
int view_lamps_count(void);             /* the lamps the track file names */
int view_lamps_of(const FshEntry *texture);                  /* the lamp `texture` is, or -1 */
uint8_t (*view_lamps_table(int lamp))[256];            /* the lamp's darkening tables */
void view_lamps_frame_start(void);                           /* no halo noted yet */
void view_lamps_note(const FshEntry *texture, const int32_t uv[8], const int32_t screen[8]);   /* a lamp polygon drawn */
void view_lamps_note_full(const FshEntry *texture, const int32_t screen[8]);
void view_lamps_textures(const TrackArt *art);               /* the textures and colours when the light's tables are not made */
void view_lamps_make_tables(const TrackArt *art, const uint8_t (*tunnel_tables)[256]);
void view_lamps_draw(const QuadTarget *t, const uint8_t palette[768]);   /* the halos noted this frame, over the finished 3D */

#endif
