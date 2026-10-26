/* port: the port's own settings (port.cfg), kept apart from the game's config.dat. */
#ifndef TNFS_PORT_SETTINGS_H
#define TNFS_PORT_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>

#include "game/platform/platform.h"

typedef enum { SCREEN_4_3 = 0, SCREEN_16_9 = 1, SCREEN_21_9 = 2 } ScreenFormat;

enum { PSX_DIR_MAX = 256 };   /* port: the longest psx_dir kept; a longer one is cut */

/* port: the PS1 time of day the light and its ambient colour come from (the letter of Z<TRACK><letter>.LGT / .LGS is 'B', 'A', 'C') */
enum { PSX_TIME_DAY = 0, PSX_TIME_DAWN = 1, PSX_TIME_DUSK = 2 };

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
    int psx_shadow;          /* 1: the PS1 look's soft shadow under each car, weaker in the dark (soft_shadow.h); 0 the exe's quad */
    int psx_time;            /* with psx_light: the PS1's time of day, PSX_TIME_DAWN (files A), PSX_TIME_DAY (B) or PSX_TIME_DUSK (C):
                             * `psx_time=dawn|day|dusk` (a, b, c also read) */
    int weather;             /* WEATHER_* (weather.h): `weather=clear|fog|rain`, re/notes/weather-design.md; clear */
} PortSettings;

/* The scale the race renders at: TNFS_RENDER_SCALE (1..3) when set, else port.cfg's render_scale. */
int port_render_scale(void);

/* 1 when the race draws with the PS1's light: TNFS_PSX_LIGHT=0/1 when set, else port.cfg's psx_light. */
int port_psx_light(void);
/* 1 when the cars have the PS1 look's soft shadow: TNFS_PSX_SHADOW=0/1 when set, else port.cfg's psx_shadow. */
int port_psx_shadow(void);
/* The time of day (PSX_TIME_*): TNFS_PSX_TIME=dawn|day|dusk when set, else port.cfg's psx_time. */
int port_psx_time(void);
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
