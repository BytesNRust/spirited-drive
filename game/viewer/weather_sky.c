#include "game/viewer/weather_sky.h"

#include "game/race/weather.h"
#include "game/sys/fixed_math.h"
#include "game/sys/port_settings.h"
#include "game/viewer/drive_light.h"
#include "game/viewer/modern_road.h"
#include "game/viewer/weather_draw.h"

#include <math.h>
#include <string.h>

const WeatherTables *weather_tables_for(const TrackArt *art, int weather, int snow, int variant, const ModernLook *look)
{
    static WeatherTables wt;
    float tint[3];
    drive_light_tint(tint);
    const int sample = port_fog_sample(), lift = port_fog_lift(), dark = look->dark;
    if (wt.made && wt.weather == weather && wt.snow == snow && wt.variant == variant && wt.sample == sample && wt.lift == lift && wt.dark == dark &&
        memcmp(wt.tint, tint, sizeof tint) == 0 && memcmp(wt.palette, art->palette, sizeof wt.palette) == 0)
        return &wt;
    static uint8_t scratch[MODERN_FOG_LEVELS][256];
    uint8_t base[3], sky[3], rain[3];
    if (sample) modern_look_fog_colour_sampled(art, lift, base);
    else modern_look_fog_colour(art, base);
    weather_fog_colour_for(weather, snow, variant, base, wt.fog);
    weather_fog_for_tint(wt.fog, tint, wt.fog);   /* the time of day's tint reaches the fog at half strength */
    modern_look_weather_fog(look, wt.fog, wt.fog);   /* the dark track's fog as its distance fog: the rain's haze too */
    weather_sky_colour_for(weather, snow, variant, wt.fog, sky);
    modern_look_weather_fog(look, sky, sky);          /* the rain's own sky colour: dark as well */
    modern_fog_tables(art->palette, sky, scratch);
    const int wet_sky = weather_sky_is_wet_dark(weather, snow, variant);   /* port (weather_fog): a snow white sky is the fog's, fading to the horizon, like the fog weather's */
    wt.sky_low = wet_sky ? WEATHER_RAIN_SKY_LEVEL : WEATHER_SKY_LEVEL;
    wt.sky_high = wet_sky ? wt.sky_low : MODERN_FOG_LEVELS - 1;
    memcpy(wt.sky, scratch[wt.sky_low], sizeof wt.sky);
    memcpy(wt.levels, scratch, sizeof wt.levels);
    weather_rain_colour(rain);
    modern_fog_tables(art->palette, rain, scratch);
    memcpy(wt.rain, scratch[WEATHER_STREAK_LEVEL], sizeof wt.rain);
    for (int l = 0; l < WEATHER_RAIN_LEVELS; l++) memcpy(wt.rain4[l], scratch[weather_rain_level_fog(l)], 256);
    { const uint8_t dark[3] = { 20, 26, 36 };
      modern_fog_tables(art->palette, dark, scratch);
      memcpy(wt.lens, scratch[6], 256);   /* port (rain v3) */
      const uint8_t grey[3] = { 92, 98, 108 }, red[3] = { 255, 34, 24 };   /* port (rain v4) */
      modern_fog_tables(art->palette, grey, scratch);
      memcpy(wt.mood, scratch[drive_light_night() ? 4 : 5], 256);
      modern_fog_tables(art->palette, red, scratch);
      memcpy(wt.tail, scratch[9], 256);
      const uint8_t warm[3] = { 255, 226, 160 };
      modern_fog_tables(art->palette, warm, scratch);
      memcpy(wt.head, scratch[9], 256); }
    weather_flash_colour(rain);
    modern_fog_tables(art->palette, rain, scratch);
    for (int l = 0; l <= WEATHER_FLASH_PEAK; l++) memcpy(wt.flash[l], scratch[l], 256);
    memcpy(wt.palette, art->palette, sizeof wt.palette);
    memcpy(wt.tint, tint, sizeof tint);
    wt.weather = weather, wt.snow = snow, wt.variant = variant, wt.sample = sample, wt.lift = lift, wt.dark = dark, wt.made = 1;
    return &wt;
}

/* the weather's sky for the view `view` at the camera matrix `matrix` (the world's up taken to the camera's space) */
WeatherSky weather_sky(uint8_t *pixels, int32_t pitch, int32_t left, int32_t right, const WeatherTables *wt,
                       const View *view, const int32_t matrix[9], int flash)
{
    const int32_t up_world[3] = { 0, 0x10000, 0 };
    int32_t c[3];
    mat3_apply(up_world, matrix, c);
    const float len = sqrtf((float)c[0] * (float)c[0] + (float)c[1] * (float)c[1] + (float)c[2] * (float)c[2]);
    const float k = len > 0 ? 1.0f / len : 0.0f;
    return (WeatherSky){ pixels, pitch, left, right, wt, view->centre_x, view->centre_y, (float)view->focal_x,
                         { (float)c[0] * k, (float)c[1] * k, (float)c[2] * k }, flash };
}

void weather_sky_band(void *user, int top, int bottom)
{
    const WeatherSky *s = user;
    if (s->wt->sky_high > s->wt->sky_low)
        weather_sky_fade(s->pixels, s->pitch, s->left, s->right, top, bottom, s->wt->levels, s->wt->sky_low, s->wt->sky_high,
                         s->cx, s->cy, s->focal, s->up);
    else weather_sky_rows(s->pixels, s->pitch, s->left, s->right, top, bottom, s->wt->sky);
    const int flash = s->flash;   /* the lightning: the sky, drawn before the scenery, goes (nearly) white, the silhouettes of what stands against it kept */
    if (flash > 0) weather_sky_rows(s->pixels, s->pitch, s->left, s->right, top, bottom, s->wt->flash[flash]);
}

void weather_style(const TrackArt *art, const WeatherTables *wt, int weather, int snow, int variant, ModernLook *look, int look_filter, int rain_open)
{
    WeatherFogShape shape;
    weather_fog_shape(weather, snow, variant, &shape);   /* the fog weather's, the rain's, and (weather_fog) the snow's and the storm's */
    ModernStyle style = *modern_look_fixed_fog(look, art, look_filter, wt->fog, shape.start, shape.end);
    style.fog_exp = shape.exp, style.fog_cap = shape.cap;   /* the exponential variant (weather.h), where the shape has one */
    if (!weather_fog_open(rain_open, style.fog_start, &style.fog_end, &style.fog_cap)) style.fog = NULL;   /* port (weather_zones): no fog inside a closed zone, thinning at its mouths */
    modern_road_set_style(&style);
}
