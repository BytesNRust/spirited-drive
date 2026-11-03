/* port: the port's own settings (port.cfg), kept apart from the game's config.dat. */
#ifndef TNFS_PORT_SETTINGS_H
#define TNFS_PORT_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>

#include "game/platform/platform.h"

typedef enum { SCREEN_4_3 = 0, SCREEN_16_9 = 1, SCREEN_21_9 = 2 } ScreenFormat;

enum { PSX_DIR_MAX = 256 };   /* port: the longest psx_dir kept; a longer one is cut */

/* port: the PS1 time of day the light and its ambient colour come from (the letter of Z<TRACK><letter>.LGT / .LGS is 'B', 'A', 'C') */
enum { PSX_TIME_DAY = 0, PSX_TIME_DAWN = 1, PSX_TIME_DUSK = 2, PSX_TIME_NIGHT = 3 };   /* the night: the dusk's files and sky, under a darker light and a dark fog (drive_light.c) */

typedef struct {
    ScreenFormat format;     /* SCREEN_4_3 */
    int refresh_hz;          /* 60, 120 or 144; 60 */
    int scale_integer;       /* 0 fill, 1 whole multiples only; 0 */
    int fullscreen;          /* 0 */
    int fov_adjust;          /* degrees added to the vertical field, -10..+20; 0 */
    int view_ultra;          /* 1: the "ultra" view distance; 0 */
    /* the three that moved in from EXTRAS when the graphics settings were given their own screen. Still read from
     * extras.cfg when port.cfg has no key for them (see port_settings_load), so a file written before the move keeps
     * working; a key in port.cfg always wins, so saving here is never undone by a stale extras.cfg. */
    int perspective;         /* 0/1: "Perspective Correction", the ex-EXTRAS Modern 3D */
    int texture_filter;      /* 0/1 */
    int fog;                 /* 0/1 */
    int f3_ultra;            /* 1: F3 rotates Far, Medium, Close and Ultra (port_f3_ultra); 0 the exe's three; 1 */
    int render_scale;        /* 1, 2 or 3: the 3D world drawn at that many times the frame's size (render-scale-design.md); 1 */
    int laps;                /* 0 the exe's laps, else 1..99 laps in every race (free_laps.h): `laps=N` */
    /* the folder of the PS1 CD's extracted car files (Z<CAR>.VSM / .WSM) for extras.cfg's psx_liveries; "" none, and
     * the liveries stay off. Read at each race start. */
    char psx_dir[PSX_DIR_MAX];
    int psx_light;           /* 1: the PS1's baked light on bridges and tunnels, read from psx_dir (psx_light.h); 0 off */
    int psx_headlights;      /* with psx_light at night: 1 the night's second look, the road dark and lit by a pool ahead of each car (headlight_pool.h); 0 the first look */
    int psx_shadow;          /* 1: the PS1 look's soft shadow under each car, weaker in the dark (soft_shadow.h); 0 the exe's quad */
    int psx_time;            /* with psx_light: the PS1's time of day, PSX_TIME_DAWN (files A), PSX_TIME_DAY (B) or PSX_TIME_DUSK (C):
                             * `psx_time=dawn|day|dusk` (a, b, c also read) */
    int weather;             /* WEATHER_* (weather.h): `weather=clear|fog|rain`, re/notes/weather-design.md; clear */
    /* the render threads: 0 off (the plain path, as the exe draws it), -1 "auto" (as many as the logical cores,
     * at most eight), or a thread count. The TNFS_BANDS environment variable wins over this one. */
    int render_bands;        /* -1 (auto) */
    int body_shine;          /* 1 subtle, 2 the default look, 3 strong: the horizon shine over the cars' paint (body_shine.h, re/TODO.md); 4 the envmap (car_envmap.h, Gran Turismo style streaks); 0 the exe's frames */
    int free_records;        /* 1: a free race (laps=N) of a lap count the track lacks keeps its own table of the ten best totals
                              * (free_records.h); 0: no total for it, as the exe has no table */
    int car_colours;         /* 1: the screen palette's slots the cars own are filled with the colours the cars show most (race_palette_fit);
                              * 0: the exe's own slots, whose nearest colours turn a body's light tones to grey */
    int fog_sample;          /* 1: the distance fog's colour from the horizon art's band around its line (modern_look.h); 0: from the
                              * art's last rows, as it was */
    int made_tunnels;        /* 1: with the Light Effects switch, a track whose light is made (no PS1 file: TR6, TR7) takes its tunnels from the road's property bit, so the
                              * tunnel tables (grey floor, warm cast) and the tunnel's light apply there as on the PS1 tracks; 0: as before */
    int psx_flares;          /* 1: with the Light Effects switch, a soft halo round a lit brake light in low light (dawn, dusk, night, a tunnel) and in fog
                              * (lamp_flare.h); 0: the exe's */
    int headlight_relight;   /* 1: with the night pools, the road's vertex light is asked again of the pools every frame, not only when the camera changes segment (the beam
                              * lagged and jumped every four nodes); 0: as before */
    int car_tunnel_light;    /* 1: with the PS1 light on a made-light track (TR6, TR7) that has the road's tunnel flags (made_tunnels), the car's light is the tunnel's alone, with a
                              * short ramp at the mouths: the terrain's shade (a canyon) no longer darkens it into the tunnel's light and lamps, and TR6's property-16 stretch after the
                              * mine is no longer taken for a hall (only TR7 has one); 0: as it was */
    int vegas_car_night;     /* 1: with the Light Effects switch and the night, Lost Vegas with no PS1 light takes its night tint under the same ceiling the CD's nights take,
                              * so the car is no longer lit as by day in its dark; 0: the fallback's night tint as it was */
    int rally_gravel_shoulder; /* 1: with extras.cfg's rally_gravel, the rolling gravel's one-shot is the shoulder's own (same sample, same pitch, about one a second) and not the
                              * faster double strike that rang like drops; 0: as it was */
    int hall_light;          /* 1: with the PS1 light on a made-light track (TR6, TR7), the car and the soft shadow in TR7's foundry hall take the scenery's own
                              * light there, not the tunnel's dark; 0: the hall is a tunnel to the car, as it was */
    int sky_fill;            /* 1: the flat sky's art extended upward until it reaches the view's top (sky_stretch.h);
                              * 0: the exe's, a flat colour above the art's top */
    int night_sky_hue;       /* 1: the night's sky of the dusk's art takes a blue-black hue at its own luminance (psx_sky_draw_rehue); 0: the art's
                              * hues under the night's tint, as it was */
    int forest_dark;         /* 1: Vertigo Ridge's forest before the finish alternates two dark tones (psx_light_forest); 0: the made light as it was */
    int night_stretch_clean; /* 1: the night's sky carried up to the view's top does not carry a star up as a stripe (sky_stretch.h, SKY_FILL_COLUMN_QUIET); 0: as it was */
    int replay_no_ffb;       /* 1: a replay does not move the force-feedback stick or the pad's rumble (race_forces_replay_silent); 0: the exe's, it shakes as in the race */
    int last_camera;         /* 1: a race starts in the camera the player last used (the intro's fly-over lands on it, not on the dash): the game writes camera_kind itself; 0: the exe's, the dash */
    int camera_kind;         /* the chase kind last used live (0..5, race_chase.h), -1 none yet; read only with last_camera */
    int sky_fill_below;      /* 1: the flat sky's art extended downward until it reaches the bottom of the view, every column in the colour its last rows share (sky_stretch_below), 16:9 and 21:9 only; 0: the exe's, one flat colour from the art's last pixel */
    int radar_all_views;     /* 1: EXTRAS' police radar (Bumper Radar) shows in every view outside the car, not only the bumper's (drive_radar_view); 0: the bumper view only, as it was */
    int tach_dim;            /* 1: the R&T tachometer of the views outside the car takes the dash's half tint with the time of day (a little darker at night), its needle staying lit (tach_dim.h); 0: untinted, as it was */
    int gauge_wheel;         /* 1: at night the steering wheel in front of the gauges is the dash's (half tint) like the rest of the wheel, not lit with the dials; 0: lit, as it was */
    int profile_ui;          /* 1 (the port's default): Game Mode and Options show the Profile row (profile.h); 0: the exe's menus, no row. `port_profile_ui=0|1`. Not controlled by a profile. */
    int profile;             /* a Profile (port_profile.h): PROFILE_CUSTOM 0 (no `profile=` line) keeps the values below as they are; another takes the settings it controls from its table
                              * at load. `profile=custom|stock|restored|restomod`. Last, so the positional initialisers keep their meaning. */
} PortSettings;

/* The scale the race renders at: TNFS_RENDER_SCALE (1..3) when set, else port.cfg's render_scale. */
int port_render_scale(void);

/* 1 when the race draws with the PS1's light: TNFS_PSX_LIGHT=0/1 when set, else port.cfg's psx_light. */
int port_psx_light(void);
/* 1 when the cars have the PS1 look's soft shadow: TNFS_PSX_SHADOW=0/1 when set, else port.cfg's psx_shadow. */
int port_psx_shadow(void);

/* 1 when the night is lit by the cars' headlight pools: TNFS_PSX_HEADLIGHTS=0/1 when set, else port.cfg's psx_headlights. */
int port_psx_headlights(void);
/* 1 when a free race's total goes in the per-lap-count tables (free_records.h): TNFS_FREE_RECORDS=0/1 when set, else port.cfg's free_records. */
int port_free_records(void);
/* 1 when the cars' palette slots are fitted to the colours they show (race_palette.h): TNFS_CAR_COLOURS=0/1 when set, else port.cfg's car_colours. */
int port_car_colours(void);
/* 1 when the distance fog takes its colour from the band around the horizon's line: TNFS_FOG_SAMPLE=0/1 when set, else port.cfg's fog_sample. */
int port_fog_sample(void);
/* the percent (0..100) of the way to white that band's colour is lifted: TNFS_FOG_LIFT when set, else FOG_SAMPLE_LIFT_DEFAULT. */
int port_fog_lift(void);
enum { FOG_SAMPLE_LIFT_DEFAULT = 15 };
/* 1 when Game Mode and Options show the Profile row: TNFS_PROFILE_UI=0/1 when set, else port.cfg's port_profile_ui (default 1). */
int port_profile_ui(void);

/* 1 when the flat sky's art reaches the top of the view (sky_stretch.h): TNFS_SKY_FILL=0/1 when set, else port.cfg's sky_fill. */
int port_sky_fill(void);
/* 1 when the wheel in front of the gauges is not lit with them: TNFS_GAUGE_WHEEL=0/1 when set, else port.cfg's gauge_wheel. */
int port_gauge_wheel(void);
/* 1 when the outside views' tachometer is tinted: TNFS_TACH_DIM=0/1 when set, else port.cfg's tach_dim. */
int port_tach_dim(void);
/* 1 when the flat sky's art reaches the bottom of the view: TNFS_SKY_FILL_BELOW=0/1 when set, else port.cfg's sky_fill_below; never in 4:3 (sky_fill_below_applies). */
int port_sky_fill_below(void);
/* the flag `s` has, and the format it is for: on, and not 4:3, where the exe's fill stays whatever the flag says */
int sky_fill_below_applies(const PortSettings *s);
/* 1 when the police radar shows in every outside view: TNFS_RADAR_ALL_VIEWS=0/1 when set, else port.cfg's radar_all_views. */
int port_radar_all_views(void);
/* 1 when the night's sky is re-hued blue-black (psx_sky_draw.h): TNFS_NIGHT_SKY_HUE=0/1 when set, else port.cfg's night_sky_hue. */
int port_night_sky_hue(void);
/* 1 when Vertigo Ridge's forest is sombre (psx_light_make.h): TNFS_FOREST_DARK=0/1 when set, else port.cfg's forest_dark. */
int port_forest_dark(void);
/* 1 when the night's stretched sky leaves its stars out of the fill: TNFS_NIGHT_STRETCH_CLEAN=0/1 when set, else port.cfg's night_stretch_clean. */
int port_night_stretch_clean(void);
/* 1 when a replay leaves the force feedback still: TNFS_REPLAY_NO_FFB=0/1 when set, else port.cfg's replay_no_ffb. */
int port_replay_no_ffb(void);
/* 1 when a race starts in the last camera used: TNFS_LAST_CAMERA=0/1 when set, else port.cfg's last_camera. */
int port_last_camera(void);
/* 1 when a lit brake light has its halo (lamp_flare.h): TNFS_PSX_FLARES=0/1 when set, else port.cfg's psx_flares; never without port_psx_light. */
int port_psx_flares(void);

/* 1 when a made light marks its tunnels from the road (made_tunnels): TNFS_MADE_TUNNELS=0/1 when set, else port.cfg's made_tunnels; never without port_psx_light. */
int port_made_tunnels(void);
/* 1 when the scenery's open models draw from behind too: TNFS_TWO_SIDED=0/1 (dev) when set, else `automatic`: the race has a reversed track or the 360 camera
 * on (both show the scenery from behind), decided when the track's models are made. No port.cfg key (an old two_sided_scenery line is ignored). */
int port_two_sided_scenery(int automatic);
/* 1 when the L buildings (the two-sided boards) are closed into boxes (track_object_draw.h): TNFS_CLOSE_BOXES=0/1 (dev, for A/B) when set, else `automatic`, the same
 * condition as the open models drawn from behind: a reversed track or the 360 camera. No port.cfg key, no menu. */
int port_close_boxes(int automatic);
/* 1 when a reversed circuit is turned fully (tri_reverse_with): the tunnel/LOD byte goes with its strip and the 3D models (tunnel portals, bridges) turn about the centre of their footprint, not their corner: TNFS_REVERSE_TUNNELS=0/1 (dev), default 0 (the file as tri_reverse makes it). No port.cfg key yet. */
int port_reverse_tunnels(void);
/* 1 when the foundry hall's car and shadow take the scenery's light (psx_light_synth.h): TNFS_HALL_LIGHT=0/1 when set, else port.cfg's hall_light; never without port_psx_light. */
int port_hall_light(void);

/* 1 when the road's light is asked again each frame (headlight_relight): TNFS_HEADLIGHT_RELIGHT=0/1 when set, else port.cfg's; never without port_psx_headlights. */
int port_headlight_relight(void);

/* 1 when the car's light of a made light is the tunnel's alone (car_tunnel_light): TNFS_CAR_TUNNEL_LIGHT=0/1 when set, else port.cfg's; never without port_psx_light. */
int port_car_tunnel_light(void);
/* 1 when Lost Vegas' made night has the darker tint (vegas_car_night): TNFS_VEGAS_CAR_NIGHT=0/1 when set, else port.cfg's; never without port_psx_light. */
int port_vegas_car_night(void);
/* 1 when the rolling rally gravel strikes as the shoulder does (rally_gravel_shoulder): TNFS_RALLY_GRAVEL_SHOULDER=0/1 when set, else port.cfg's. */
int port_rally_gravel_shoulder(void);

/* how sky_fill fills the rows above the art (sky_stretch.h's SKY_FILL_*): TNFS_SKY_FILL_MODE=a|b (or 1|2) when set, else the default. */
enum { SKY_FILL_MODE_DEFAULT = 1 };
int port_sky_fill_mode(void);
/* The time of day (PSX_TIME_*): TNFS_PSX_TIME=dawn|day|dusk when set, else port.cfg's psx_time. */
int port_psx_time(void);
/* 1 when the masked lamp texels glow in low light (lamp_emissive.h): the Graphics Light Effects switch (port_psx_light), unless TNFS_EMISSIVE_LAMPS=0/1 is set. */
int port_emissive_lamps(void);

/* the cars' body shine strength, 0 (the exe's frames) to 4: TNFS_BODY_SHINE=0..4 when set, else port.cfg's body_shine. */
int port_body_shine(void);
/* The weather of the race: TNFS_WEATHER=clear|fog|rain when set, else port.cfg's weather (a WEATHER_* of weather.h). */
int port_weather(void);

/* The profile port.cfg names (PROFILE_CUSTOM without the line or the file), read from the file itself (port_extras_load takes it from here, so it cannot go through
 * port_settings_load, which reads extras.cfg). */
int port_settings_profile(void);
/* Chooses profile `p` (a Profile): port.cfg's name and the settings it controls in port.cfg and extras.cfg set to its table (PROFILE_CUSTOM: only the name, the values stay).
 * False when a file does not take it. */
bool port_profile_select(int p);
/* The settings in port.cfg and extras.cfg changed by hand: a profile whose table they no longer match becomes custom (port_settings_save and port_extras_save do it themselves;
 * this is for the menus that rewrite one by another way). */
bool port_settings_profile_custom(void);
/* Where the display's size comes from (the program sets platform_desktop_size; without one, or when it cannot tell, port_monitor_format gives its fallback). */
void port_settings_set_display_probe(bool (*probe)(int *width, int *height));
/* The widescreen format of the display the window is on (4:3 on a 4:3 one); `fallback` when it cannot be told. */
ScreenFormat port_monitor_format(ScreenFormat fallback);

PortSettings port_settings_defaults(void);
/* "key=value" per line; unknown keys and out-of-range values are ignored (the default stays). */
PortSettings port_settings_parse(const char *text, size_t size);
/* Writes the text form, NUL-terminated; returns its length, or 0 when it does not fit. */
/* the room port.cfg's text needs: every key, a psx_dir of PSX_DIR_MAX bytes, and space for the keys still to come */
enum { PORT_CFG_TEXT_MAX = 2048 };
size_t port_settings_format(const PortSettings *s, char *out, size_t size);
PortSettings port_settings_load(void);   /* user file "port.cfg"; the defaults when it does not exist */
bool port_settings_save(const PortSettings *s);
/* The platform's config for the saved settings: the format's width by 480 lines, scaling, full screen, refresh. */
PlatformConfig port_platform_config(const char *title);

#endif
