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
#include "game/viewer/drive_bumper.h"

#include <math.h>
#include <stdint.h>
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
static int wiper_glass_w;

void ported_view_set_wiper_glass(const uint8_t *glass, int width, float x, float y, float scale)
{
    wiper_glass = glass, wiper_glass_w = width, wiper_glass_x = x, wiper_glass_y = y, wiper_glass_scale = scale > 0.0f ? scale : 1.0f;
}

static const uint8_t *wiper_nose;
static int wiper_nose_box[4];

void ported_view_set_wiper_nose(const uint8_t *nose, const int box[4])
{
    wiper_nose = box != NULL ? nose : NULL;
    if (wiper_nose != NULL) memcpy(wiper_nose_box, box, sizeof wiper_nose_box);
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
    out->glass = wiper_glass, out->glass_x = wiper_glass_x - (float)box[0], out->glass_y = wiper_glass_y - (float)box[1], out->glass_scale = wiper_glass_scale, out->glass_w = wiper_glass_w;
    out->nose = wiper_glass != NULL ? wiper_nose : NULL;
    memcpy(out->nose_box, wiper_nose_box, sizeof out->nose_box);
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

/* the followed car's view is the dash (DRIVE_DASH_KIND): the rain on the roof, the roll muffled */
static int in_cockpit(const RaceState *race) { return race->chase.kind == DRIVE_DASH_KIND; }

/* the ground the rain's moving layer is heard on (rain_roll.h): none in the air, on grass or a shoulder; on the rally, its gravel where the gravel roll is (road_noise, race_sound_engine.c: off
 * the shoulder's node flags); else the asphalt of an on-road surface */
static int wet_ground(const RaceState *race, const Car *car)
{
    if (car->airborne_ticks != 0) return RAIN_ROLL_OFF;
    if (race->port_rally_gravel != 0) return car->node_flag_4aa == 0 && car->node_flag_4c6 == 0 ? RAIN_ROLL_GRAVEL : RAIN_ROLL_OFF;
    return (uint32_t)car->surface < RACE_SURFACES && race->surfaces[car->surface].off_road == 0 ? RAIN_ROLL_ASPHALT : RAIN_ROLL_OFF;
}

/* the camera's eye from the followed car, 16.16 metres (the chase's position is the eye as last placed, the sounds' listener) */
static int32_t eye_distance(const RaceState *race, const Car *car)
{
    const double dx = (double)race->chase.position[0] - car->x, dy = (double)race->chase.position[1] - car->y, dz = (double)race->chase.position[2] - car->z;
    const double d = sqrt(dx * dx + dy * dy + dz * dz);
    return d > INT32_MAX ? INT32_MAX : (int32_t)d;
}

/* the moving layer's level for the view (rain_roll_view_permille), glided over the race's ticks on a change of view; a new race, a clock gone back or a jump of more than a few seconds
 * takes the view's level at once. Kept here only as the voice's state: the view and its distance are read each frame. */
static int view_level(const RaceState *race, const Car *car)
{
    static uint32_t last;
    static int level, have;
    const int target = rain_roll_view_permille(in_cockpit(race), eye_distance(race, car));
    const uint32_t tick = (uint32_t)race->tick;
    if (!have || tick < last || tick - last > 3 * RACE_TICKS_A_SECOND) level = target, have = 1;
    else level = rain_roll_view_glide(level, target, tick - last);
    last = tick;
    return level;
}

/* port (rain_tyre_roll): what the car the camera follows hears of the rain (as the tyre sounds are), once a frame (rain_roll.h), in rain or a storm with rain_v2: the stopped layer (outside or in
 * the cockpit, rain only: falling snow is silent) giving way to the wet road by its speed, on asphalt, or under the gravel roll on the rally, at the view's level (view_level); all by the openness
 * (silent inside a closed zone). The snowy
 * rally's track is snow, not water: none of it there (snow_roll.h). The voices fall silent by themselves when the frames stop. */
void weather_hooks_rain_roll(const WeatherFrame *f)
{
    RainRoll r = rain_roll_levels(RAIN_ROLL_DRY, 0, 0, RAIN_ROLL_OFF, 0);
    if (f->race != NULL && port_rain_tyre_roll() && port_rain_v2() && weather_wet(f->weather) && f->race->port_rally_gravel != RALLY_GRAVEL_SNOW) {
        const Car *const car = f->race->cars[race_slot(f->race->viewed)];
        if (car != NULL) {
            r = rain_roll_levels(f->snow_level != SNOW_NONE ? RAIN_ROLL_SNOW : RAIN_ROLL_RAIN, f->rain_open, car->speed, wet_ground(f->race, car), in_cockpit(f->race));
            r.moving = (r.moving * view_level(f->race, car) + 500) / 1000;   /* the cockpit 3 dB lower, the chase cameras lower the farther */
        }
    }
    drive_audio_rain_roll(r.out, r.in, r.moving, r.pitch);
}

/* port (snow): the roll of the car the camera follows on the snowy rally's track (RALLY_GRAVEL_SNOW), once a frame: its level and pitch by its speed (snow_roll.h), by the openness where snow falls,
 * 6 dB lower in the cockpit; the exe's gravel sample is not struck there (race_sound.c). Snow falling elsewhere is the rain's wet road (rain_roll.h). The voice falls silent by itself when the frames stop. */
void weather_hooks_snow_roll(const WeatherFrame *f)
{
    int percent = 0, pitch = SNOW_ROLL_PITCH_SLOW;
    if (f->race != NULL && f->race->port_rally_gravel == RALLY_GRAVEL_SNOW) {
        const Car *const car = f->race->cars[race_slot(f->race->viewed)];
        if (car != NULL) {
            percent = snow_roll_volume(1, f->snow_level != SNOW_NONE ? f->rain_open : 16, car->speed, car->airborne_ticks == 0, in_cockpit(f->race));
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


