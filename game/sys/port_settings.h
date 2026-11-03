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
    int psx_flares;          /* 1: with the Light Effects switch, a soft halo round a lit brake light in low light (dawn, dusk, night, a tunnel) and in fog
                              * (lamp_flare.h); 0: the exe's */
    int sky_fill;            /* 1: the flat sky's art extended upward until it reaches the view's top (sky_stretch.h);
                              * 0: the exe's, a flat colour above the art's top */
    int last_camera;         /* 1: a race starts in the camera the player last used (the intro's fly-over lands on it, not on the dash): the game writes camera_kind itself; 0: the exe's, the dash */
    int camera_kind;         /* the chase kind last used live (0..5, race_chase.h), -1 none yet; read only with last_camera */
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
/* 1 when the flat sky's art reaches the top of the view (sky_stretch.h): TNFS_SKY_FILL=0/1 when set, else port.cfg's sky_fill. */
int port_sky_fill(void);
/* 1 when a race starts in the last camera used: TNFS_LAST_CAMERA=0/1 when set, else port.cfg's last_camera. */
int port_last_camera(void);
/* 1 when a lit brake light has its halo (lamp_flare.h): TNFS_PSX_FLARES=0/1 when set, else port.cfg's psx_flares; never without port_psx_light. */
int port_psx_flares(void);
/* 1 when the scenery's open models draw from behind too: TNFS_TWO_SIDED=0/1 (dev) when set, else `automatic`: the race has a reversed track or the 360 camera
 * on (both show the scenery from behind), decided when the track's models are made. No port.cfg key (an old two_sided_scenery line is ignored). */
int port_two_sided_scenery(int automatic);
/* 1 when the L buildings (the two-sided boards) are closed into boxes (track_object_draw.h): TNFS_CLOSE_BOXES=0/1 (dev, for A/B) when set, else `automatic`, the same
 * condition as the open models drawn from behind: a reversed track or the 360 camera. No port.cfg key, no menu. */
int port_close_boxes(int automatic);
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

PortSettings port_settings_defaults(void);
/* "key=value" per line; unknown keys and out-of-range values are ignored (the default stays). */
PortSettings port_settings_parse(const char *text, size_t size);
/* Writes the text form, NUL-terminated; returns its length, or 0 when it does not fit. */
size_t port_settings_format(const PortSettings *s, char *out, size_t size);
PortSettings port_settings_load(void);   /* user file "port.cfg"; the defaults when it does not exist */
bool port_settings_save(const PortSettings *s);
/* The platform's config for the saved settings: the format's width by 480 lines, scaling, full screen, refresh. */
PlatformConfig port_platform_config(const char *title);

#endif
