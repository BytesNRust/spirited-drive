#include "game/viewer/weather_hooks.h"

#include "game/race/rain_roll.h"
#include "game/race/rally_gravel.h"
#include "game/race/snow_roll.h"
#include "game/race/spray_tint.h"
#include "game/race/tick_rate.h"
#include "game/race/weather.h"
#include "game/race/weather_snow.h"
#include "game/sys/port_settings.h"
#include "game/viewer/drive_audio.h"

#include <string.h>

/* port (wipers): the dash view's blades (the 3D buffer's pixels), what falls fixes their pace, and the darkening table the blades are drawn with */
static WiperBlade wiper_scene[WIPER_BLADES];
static int wiper_scene_count;

void ported_view_set_wipers(const WiperBlade *blades, int count)
{
    wiper_scene_count = blades != NULL && count > 0 ? (count < WIPER_BLADES ? count : WIPER_BLADES) : 0;
    if (wiper_scene_count > 0) memcpy(wiper_scene, blades, sizeof(WiperBlade) * (size_t)wiper_scene_count);
}

static const uint8_t *wiper_glass;
static float wiper_glass_x, wiper_glass_y, wiper_glass_scale = 1.0f;

void ported_view_set_wiper_glass(const uint8_t *glass, float x, float y, float scale)
{
    wiper_glass = glass, wiper_glass_x = x, wiper_glass_y = y, wiper_glass_scale = scale > 0.0f ? scale : 1.0f;
}

int weather_hooks_wiper_blades(void) { return wiper_scene_count; }

/* the passes in the box's own pixels for this frame, NULL when there are none (no blades, nothing falls) */
const WiperPass *weather_hooks_wiper_pass(WiperPass *out, const int32_t box[4], const WeatherFrame *f)
{
    if (wiper_scene_count == 0 || !weather_wet(f->weather)) return NULL;
    out->count = wiper_scene_count;
    for (int i = 0; i < wiper_scene_count; i++) {
        out->blade[i] = wiper_scene[i];
        out->blade[i].x -= (float)box[0], out->blade[i].y -= (float)box[1];
        if (out->blade[i].xmax > 0.0f) out->blade[i].xmax -= (float)box[0];
    }
    out->glass = wiper_glass, out->glass_x = wiper_glass_x - (float)box[0], out->glass_y = wiper_glass_y - (float)box[1], out->glass_scale = wiper_glass_scale;
    out->timing = wiper_timing(f->snow_level != SNOW_NONE ? WIPER_SNOW : f->weather == WEATHER_STORM ? WIPER_STORM : WIPER_RAIN);
    return out;
}

const uint8_t *weather_hooks_wiper_dark(const uint8_t palette[768])
{
    static uint8_t table[256], made_for[768];
    static int made;
    if (!made || memcmp(made_for, palette, sizeof made_for) != 0) {
        static const uint8_t black[3] = { 0, 0, 0 };
        spray_tint_table(palette, black, 97, table);
        memcpy(made_for, palette, sizeof made_for);
        made = 1;
    }
    return table;
}


/* port (rain_v2): the thunder after each strike. Every race tick since the last frame is looked at (a frame can span several); a strike schedules its clap weather_thunder_delay ticks
 * on at weather_thunder_volume. The first call, a clock gone back, or a jump of more than a few seconds starts afresh (no claps for ticks nobody saw). */
void weather_hooks_thunder(const WeatherFrame *f)
{
    static uint32_t last;
    static int have;
    if (f->weather != WEATHER_STORM || !port_rain_v2() || f->snow_level != SNOW_NONE) {   /* port (snow_weather): a snowstorm has no thunder */
        have = 0;
        return;
    }
    const uint32_t tick = (uint32_t)(f->race != NULL ? f->race->tick : f->anim_tick);
    if (!have || tick < last || tick - last > 3 * RACE_TICKS_A_SECOND) {
        last = tick, have = 1;
        return;
    }
    for (uint32_t t = last + 1; t <= tick; t++) {
        if (!weather_thunder_strike(t)) continue;
        const int delay = weather_thunder_delay(t);
        drive_audio_thunder(delay, weather_thunder_inside(weather_thunder_volume(delay), f->rain_open));
    }
    last = tick;
}

/* port (rain_tyre_roll): the wet-tyre roll of the car the camera follows (as the tyre sounds are), once a frame: its level and pitch by its speed (rain_roll.h), only in rain or a storm with rain_v2,
 * on open asphalt (not the rally's gravel, a road shoulder or the air) and by the rain's openness (silent inside a closed zone). The voice falls silent by itself when the frames stop. */
void weather_hooks_rain_roll(const WeatherFrame *f)
{
    int percent = 0, pitch = RAIN_ROLL_PITCH_SLOW;
    if (f->race != NULL && port_rain_tyre_roll() && port_rain_v2() && weather_wet(f->weather)) {
        const Car *const car = f->race->cars[race_slot(f->race->viewed)];
        if (car != NULL) {
            const int asphalt = car->airborne_ticks == 0 && f->race->port_rally_gravel == 0 && (uint32_t)car->surface < RACE_SURFACES && f->race->surfaces[car->surface].off_road == 0;
            percent = rain_roll_volume(1, f->rain_open, car->speed, asphalt);
            pitch = rain_roll_pitch(car->speed);
        }
    }
    drive_audio_wet_roll(percent, pitch);
}

/* port (snow crunch): the crunch of the car the camera follows on the snowy rally (RALLY_GRAVEL_SNOW), once a frame: its level and pitch by its speed (snow_roll.h), the gravel roll's curve; the exe's gravel
 * sample is not struck there (race_sound.c). The voice falls silent by itself when the frames stop. */
void weather_hooks_snow_roll(const WeatherFrame *f)
{
    int percent = 0, pitch = SNOW_ROLL_PITCH_SLOW;
    if (f->race != NULL && f->race->port_rally_gravel == RALLY_GRAVEL_SNOW) {
        const Car *const car = f->race->cars[race_slot(f->race->viewed)];
        if (car != NULL) {
            percent = snow_roll_volume(f->race->port_rally_gravel, car->speed, car->airborne_ticks == 0);
            pitch = snow_roll_pitch(car->speed);
        }
    }
    drive_audio_snow_roll(percent, pitch);
}

int weather_hooks_flash_level(const WeatherFrame *f)
{
    if (f->weather != WEATHER_STORM || f->snow_level != SNOW_NONE) return 0;   /* port (snow_weather): a snowstorm has no lightning */
    return weather_flash_open(weather_rain_flash((uint32_t)(f->race != NULL ? f->race->tick : f->anim_tick)), f->rain_open);   /* port (weather_zones): none inside a closed zone */
}


