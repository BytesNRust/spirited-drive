/* What drive_light.c and its split files (drive_light_sky.c, drive_light_night.c) share: the light's state and the steps between them. */
#ifndef TNFS_DRIVE_LIGHT_PRIVATE_H
#define TNFS_DRIVE_LIGHT_PRIVATE_H

#include "viewer/drive_light.h"
#include "viewer/class_overlay.h"
#include "viewer/drive_palette.h"
#include "viewer/ported_view.h"
#include "viewer/sky_art_draw.h"
#include "game/race/psx_sky.h"
#include "game/platform/platform.h"
#include "game/race/psx_lgs.h"
#include "game/race/light_tint.h"
#include "viewer/track_load.h"
#include "game/race/track_art.h"
#include "game/race/tunnel_lamp_textures.h"
#include "game/race/track_node_light.h"
#include "game/race/psx_lgt.h"
#include "game/race/street_lamp.h"
#include "game/race/track_mask.h"
#include "game/race/tunnel_lamps.h"
#include "game/race/finish_gate_light.h"
#include "game/race/window_lights.h"
#include "game/race/terrain_light.h"
#include "game/race/light_synth.h"
#include "game/race/track_light_file.h"
#include "game/race/track_geometry.h"
#include "game/race/track_objects.h"
#include "game/sys/reverse_track.h"
#include "game/sys/port_log.h"
#include "game/sys/port_profile.h"
#include "game/sys/port_settings.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>

enum { LIGHT_PATH = 320, LIGHT_TRACK = 16 };
enum { WINDOW_SEED = 0x71d0, WINDOW_ENTRIES = 1 };

extern const float NIGHT_TINT_CEILING;
void file_path(char *out, size_t size, const char *psx_dir, const char *track, char letter, const char *ext);
void load_sky(const char *psx_dir, const char *track, char letter);
void load_tint(const char *psx_dir, const char *track, char letter, int night, float ceiling);
extern TrackNodeLight light;
extern char light_letter;
extern char made_track[16];
extern const Tri *made_tri;
extern int synth_wanted, synth_on, synth_time;
extern char track_light_note[160];
extern char track_light_places[200];
extern char track_light_status[200];
extern const float NIGHT_PALETTE[3];
extern PsxLgs ambient;
extern int ambient_ok;
extern float dash_ratio[3];
extern int split;
extern float tint[3];

extern int alt_sky_wanted;
extern int dark_board_count;
extern int lamp_pool_count;
extern int night_lamp_count;
extern const TunnelLampTexture *night_lamp_spec;
extern int night_sky;
extern int sign_board_count;
extern SkyArtDraw sky_draw;
void sky_scale_night(void);
extern int street_lamp_count;
extern int street_lamp_on;
extern int window_board_count;
extern const float NIGHT_SKY_RAMP[3];
extern int headlights_on;

extern const WindowDark *dark_boards;
extern int dusk_look;
extern int night_look;
extern const WindowSign *sign_boards;
extern int street_lamp_by_day;
extern StreetLamp street_lamps[STREET_LAMPS_MAX];
extern const WindowBoard *window_boards;

#endif
