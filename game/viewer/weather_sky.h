/* port (weather): the tables a weather's picture is made with (the fog's colour, the sky's palette moved toward its colour, the rain's, the lightning's), the sky drawn
 * from them, and the modern road's style the weather asks for. WEATHER_CLEAR runs none of it. */
#ifndef TNFS_WEATHER_SKY_H
#define TNFS_WEATHER_SKY_H

#include "game/race/track_art.h"
#include "game/race/view.h"
#include "game/race/weather.h"
#include "game/viewer/modern_look.h"

#include <stdint.h>

typedef struct {
    uint8_t palette[768];
    int weather, made;
    int snow, variant;   /* port (weather_fog): what falls and the setting the fog was made for */
    int sample, lift;    /* port (fog_sample): the colour's source the fog was made from */
    int dark;            /* port (the light effects): the weather fog was made dark (modern_look_weather_fog) */
    uint8_t fog[3];      /* the fog's colour */
    uint8_t sky[256];    /* the sky's palette moved toward its colour */
    uint8_t levels[MODERN_FOG_LEVELS][256];   /* all its levels: the fog's sky gets the fog's whole at the horizon */
    int sky_low, sky_high;
    float tint[3];       /* the time of day's tint the fog colour was made for */
    uint8_t rain[256];   /* a streak's lightening */
    uint8_t flash[WEATHER_FLASH_PEAK + 1][256];   /* port (WEATHER_STORM): the picture lifted toward the lightning's colour, by the flash's brightness (0 none .. WEATHER_FLASH_PEAK all) */
    uint8_t rain4[WEATHER_RAIN_LEVELS][256];   /* port (rain_fx): and its four brightness levels, dark to white */
    uint8_t mood[256];   /* port (rain v4): the whole picture moved toward a dark grey: rain's desaturated, lower contrast light */
    uint8_t tail[256];   /* port (rain v4): a road pixel under a tail light, moved toward its red */
    uint8_t head[256];   /* port (rain_fx): a road pixel under a lit headlight, moved toward its warm white */
    uint8_t lens[256];   /* port (rain v3): the body of a drop on the lens: the scene behind it, darker */
} WeatherTables;

/* The tables of `weather`, made again when the palette, the weather, the fog's source, the time of day's tint or the dark look changed (one static set; the pointer stays valid). */
const WeatherTables *weather_tables_for(const TrackArt *art, int weather, int snow, int variant, const ModernLook *look);

typedef struct {
    uint8_t *pixels;
    int32_t pitch, left, right;   /* the columns clipped to the buffer */
    const WeatherTables *wt;
    int32_t cx, cy;               /* the view's centre and focal length, and the world's up in its space (unit) */
    float focal, up[3];
    int flash;                    /* the lightning's brightness this frame, 0 none (storm_flash_level) */
} WeatherSky;

/* the weather's sky for the view `view` at the camera matrix `matrix` (the world's up taken to the camera's space); `flash` is the lightning's brightness this frame */
WeatherSky weather_sky(uint8_t *pixels, int32_t pitch, int32_t left, int32_t right, const WeatherTables *wt, const View *view, const int32_t matrix[9], int flash);

/* one band of the sky's rows: bands_rows' callback, `user` a const WeatherSky * */
void weather_sky_band(void *user, int top, int bottom);

/* the modern road's style for the weather (fog and its zones); `rain_open` is how much weather there is where the eye is, 0..16 */
void weather_style(const TrackArt *art, const WeatherTables *wt, int weather, int snow, int variant, ModernLook *look, int look_filter, int rain_open);

#endif
