/* port: the PS1 look's light for a race (port.cfg psx_light, psx_dir): the track's Z<TRACK>B.LGT (the day), read from the
 * user's own PS1 files at the race's start (psx_light.h). Off, or with no psx_dir or no file, the race has no light and its
 * frames are the exe's. */
#ifndef TNFS_DRIVE_LIGHT_H
#define TNFS_DRIVE_LIGHT_H

#include "game/race/psx_lamps.h"
#include "game/race/track_art.h"
#include "formats/tri.h"

#include <stddef.h>
#include <stdint.h>

/* `time` is a PSX_TIME_* (port_settings.h): the letter of the files, B for the day, A for dawn, C for dusk; the ambient colour of its .LGS tints
 * the light (a missing or unreadable LGS leaves it neutral). Loads the light of `track` ("TR1") into the renderer (ported_view_set_light), or clears it; `ring_nodes` is the TRI's node_count
 * (segment_count * 4; a stage's ring_nodes is 0x20000, a mask), which the file's must match. The previous race's light is freed. */
void drive_light_load(int enabled, int time, const char *psx_dir, const char *track, uint32_t ring_nodes);

/* port: after drive_light_load, for a track the CD has no light for (Burnt Sienna, Transtropolis: psx_light_synth_track), with the light on and no light
 * loaded: the light made from the road's tunnels (psx_light_synth.h) in place of none, the time of day's flat tint kept. Anything else changes nothing. */
void drive_light_load_synth(const Tri *tri, const char *track);

/* port: the light of a track that runs the other way round (tri_reverse.h), after drive_light_load: the nodes and the sides asked as the new
 * file has them. */
void drive_light_set_reversed(int reversed);

/* After drive_light_load, with the CD's data root: the PC's alternative sky for a dawn or dusk without a PS1 one (drive_light.c) */
void drive_light_load_alt_sky(const char *data_root, const char *track);

/* The palette with every entry multiplied by the ambient colour of the light loaded (the palette itself with none), so a
 * texture keeps its variation under the tint. For what is all 3D (the track viewer); a race's frame has the HUD's pixels too, which
 * index the same entries: see below. */
/* port (night): the tunnel lamps of the track when the race is at night (how many, and `*out` their table), none otherwise. */
int drive_light_night_lamps(const PsxLamp **out);
/* port (night): the palette entries set aside for the lamps' colours (drive_palette.c), then, with the race palette built, drive_light_lamps_night fills
 * them from the lamp textures of `art` (psx_lamp_exempt, brightest first; art->palette gets the colours at the pool's entries) and the lamps' tables
 * draw those colours there. */
void drive_light_lamp_pool(const uint8_t entry[], int n);
void drive_light_lamps_night(TrackArt *art);
/* port (night, psx_headlights): the entries more the pool needs for the street lamp's flare (street_lamp.h; City's first track), else 0. */
int drive_light_street_lamp_entries(void);
void drive_light_palette(const uint8_t palette[768], uint8_t out[768]);

/* port (WEATHER_STORM): the lightning's brightness this frame (0 none .. 15), set by the render: drive_light_palette then goes that far (of 15) from the time of day's tint to the
 * palette's own colours, so the 3D, the cars too, shows full bright for the instant of a flash and decays with it. 0 leaves the palette as it was. */
void drive_light_set_flash(int level);

/* A race's frame: `frame` (count pixels) is the 3D when it is done and the 2D layer (HUD, dash, menus over it) not yet drawn. */
void drive_light_world_done(const uint8_t *frame, size_t count);
/* The 2D layer's own marks (640 x 480 wide, as the compose makes them): per layer pixel PSX_CLASS_DASH (the dash's art, the wheel, the
 * pillars, the mirror's housing: half the tint), PSX_CLASS_HUD (the top bar, the map, the gauges' dials and needles: none) or
 * LAYER_KEY (the 3D shows: whole). `width` x `height` the layer's size, `scale` the output's over it. Over what the diff finds. */
enum { PSX_CLASS_DASH = 1, PSX_CLASS_HUD = 2, PSX_CLASS_LIT = 3 };   /* LIT (port, gauge_light): a gauge's lit part at night, through its own palette (drive_light_palette_lit) */
/* port (gauge_light): the palette of the gauges' lit parts: `palette` (the plain one, as the HUD shows) brightened and tinted by gauge_light's colour (1 greenish, 2 reddish, 3 orange,
 * 4 bluish); 0 when the key is 0 or the race is not at night (`out` untouched), 1 when made. Mask value 3 goes through it. */
int drive_light_palette_lit(const uint8_t palette[768], uint8_t out[768]);
void drive_light_world_class(const uint8_t *layer, int width, int height, int scale);
/* 1 when the race loaded is at night (the night's sky and dash). */
int drive_light_night(void);
/* 1 when the gauges are lit (gauge_light): the night, and with gauge_exterior the dusk and rain with rain_lamps */
int drive_light_gauges_lit(void);
int drive_light_active(void);
/* 1 when the race was loaded with the light on and a time other than the day (dawn, dusk), or at any time with the light script read (the dash has its own
 * light even by day): its 3D and 2D are told apart, which needs the wide path. Kept at the race's start (drive_light_load), not read from port.cfg each frame. */
int drive_light_time_tinted(void);
/* The ambient tint of the light loaded, per channel (1 neutral). */
void drive_light_tint(float out[3]);   /* the tint is not neutral */
/* The ratio the dash's colours are scaled by (the light of the car the dash is of: ported_view_car_ratio, the same as the car's own, tunnel steps and
 * all); NULL, none (1). Set each frame before the palettes are made. */
void drive_light_dash_ratio(const float ratio[3]);
/* The palette with every entry half way from `palette` to drive_light_palette's, times the dash's ratio (drive_light_dash_ratio). */
void drive_light_palette_half(const uint8_t palette[768], uint8_t out[768]);
/* The rear-view mirror's picture, in the output frame `width` pixels wide: the 2D layer carries it but it shows the 3D, so it is
 * the world's (tinted); w <= 0 clears it. Set again each frame by the compose. */
void drive_light_world_rect(int width, int x, int y, int w, int h);
/* The pixels of `shown` (the frame now, count of them): 1 the 3D's (the full tint), 2 the dash's art (half), 0 the 2D's (none): the mask to
 * give platform_present_split with drive_light_palette's and _half's; NULL when there is no light script or no 3D was kept for this size. */
const uint8_t *drive_light_world_mask(const uint8_t *shown, size_t count);

#endif
