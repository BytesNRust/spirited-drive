/* the weather's tables and frame, the horizon's filter and the wide boards' draws (view_weather.c); ported_view.c calls these */
#ifndef TNFS_VIEW_WEATHER_H
#define TNFS_VIEW_WEATHER_H

#include <stdint.h>
#include "viewer/modern_look.h"
#include "viewer/view_shared.h"

typedef struct {
    uint8_t *pixels;
    int32_t pitch, box[4];
    const uint8_t *table;
    int share;
} RainMood;

extern int look_filter, look_fog;
extern PsxSkyDraw *psx_sky;
extern const TrackInfo *psx_sky_info;
extern int rain_open;
extern HorizonSmooth smooth;
extern ModernLook look;
void rain_mood_band(void *user, int top, int bottom);
int snow_level(void);
WeatherFrame weather_frame(void);
void storm_thunder_update(void);
void rain_roll_update(void);
void snow_roll_update(void);
int storm_flash_level(void);
void draw_camera(void *user, const View *view, const FshEntry *texture, const int32_t uv[8], const int32_t *const cam[4]);
void draw_full(void *user, const FshEntry *texture, const int32_t screen[8]);
void draw_full_alt(void *user, const FshEntry *texture, const int32_t screen[8]);
const WeatherTables *weather_tables(const TrackArt *art);
int wide_board_on(void);

#endif
