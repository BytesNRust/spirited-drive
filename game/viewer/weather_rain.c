/* port (rain_tyre_roll): what is heard of the rain once a frame (weather_hooks_rain_roll, weather_hooks.h): the followed car's layers (rain_roll.h) and every car's tyre spray and puddle
 * splashes (rain_cars.h), the loudest few where they are. */
#include "game/viewer/weather_hooks.h"

#include "game/audio/wet_roll.h"
#include "game/race/race_sound.h"
#include "game/race/rain_cars.h"
#include "game/race/rain_roll.h"
#include "game/race/rain_spray.h"
#include "game/race/rally_gravel.h"
#include "game/race/spray_pace.h"
#include "game/race/tick_rate.h"
#include "game/race/weather.h"
#include "game/race/weather_snow.h"
#include "game/sys/fixed_math.h"
#include "game/sys/port_log.h"
#include "game/sys/port_settings.h"
#include "game/viewer/drive_audio.h"
#include "game/viewer/drive_bumper.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

enum { OPEN_FULL = 16, DICE_SEED = 0x2545f491, TURN = 0x1000000 };

/* the followed car's view is the dash (DRIVE_DASH_KIND): the rain on the roof, the sprays muffled */
static int in_cockpit(const RaceState *race) { return race->chase.kind == DRIVE_DASH_KIND; }

/* the ground a car's spray is heard on (rain_roll.h): none in the air, on grass or a shoulder; on the rally, its gravel where the gravel roll is (road_noise, race_sound_engine.c: off
 * the shoulder's node flags); else the asphalt of an on-road surface */
static int wet_ground(const RaceState *race, const Car *car)
{
    if (car->airborne_ticks != 0) return RAIN_ROLL_OFF;
    if (race->port_rally_gravel != 0) return car->node_flag_4aa == 0 && car->node_flag_4c6 == 0 ? RAIN_ROLL_GRAVEL : RAIN_ROLL_OFF;
    return (uint32_t)car->surface < RACE_SURFACES && race->surfaces[car->surface].off_road == 0 ? RAIN_ROLL_ASPHALT : RAIN_ROLL_OFF;
}

/* the camera's eye from a car, 16.16 metres (the chase's position is the eye as last placed, the sounds' listener) */
static int32_t eye_distance(const RaceState *race, const Car *car)
{
    const double dx = (double)race->chase.position[0] - car->x, dy = (double)race->chase.position[1] - car->y, dz = (double)race->chase.position[2] - car->z;
    const double d = sqrt(dx * dx + dy * dy + dz * dz);
    return d > INT32_MAX ? INT32_MAX : (int32_t)d;
}

/* where a car is heard from, the driver's pan (race_sound_position's angle by the listener's heading, race_sound_pan), without touching the race */
static int pan_of(const RaceState *race, const Car *car)
{
    int32_t heading = race->chase.angles[1];
    if (heading < 0) heading += TURN;
    int32_t angle = heading - (fx_atan2(car->x - race->chase.position[0], car->z - race->chase.position[2]) << 8);
    if (angle < 0) angle += TURN;
    if (angle > TURN) angle -= TURN;
    return (int)race_sound_pan(angle);
}

/* the followed car's spray level for the view (rain_roll_view_permille), glided over the race's ticks on a change of view; a new race, a clock gone back or a jump of more than a few seconds
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

typedef struct {
    int spray;    /* percent, as heard */
    int puddle;   /* a splash's level as heard, percent, before its random gain; 0 no splashes */
    int pitch, pan;
    int32_t speed;
} HeardCar;

/* the cars' voices between frames */
static struct {
    RainCarVoices voices;
    SprayPace pace[RACE_CARS];
    uint32_t dice;
    int32_t last_tick;
    int last_clip, have;
} cars;

static void cars_clear(void)
{
    rain_car_voices_clear(&cars.voices);
    for (int i = 0; i < RACE_CARS; i++) spray_pace_clear(&cars.pace[i]);
    cars.dice = DICE_SEED, cars.last_clip = -1, cars.have = 0;
}

static void cars_silent(void)
{
    for (int v = 0; v < RAIN_CARS_VOICES; v++) drive_audio_rain_spray(v, 0, 1000, WET_ROLL_PAN_CENTRE, -1);
    if (cars.have) cars_clear();
}

/* what car `i` makes heard: its spray by its pace, ground and openness, by where it is heard from (the followed car's own: the view's glided level, and none in the cockpit in rain:
 * rain_moving_in.wav has it); its splashes on asphalt alone */
static HeardCar heard_car(const WeatherFrame *f, int i, int falling, int followed_level)
{
    const RaceState *const race = f->race;
    const Car *const car = race->cars[i];
    const int followed = i == race_slot(race->viewed), cockpit = in_cockpit(race);
    const int32_t speed = followed ? car->speed : spray_pace(&cars.pace[i], (int32_t)race->tick, car->x, car->z, rain_spray_speed(car->speed, car->velocity_x, car->velocity_z));
    const int ground = wet_ground(race, car), open = f->open_at != NULL ? f->open_at(car->node) : f->rain_open;
    const int level = followed ? followed_level : rain_roll_view_permille(cockpit, eye_distance(race, car));
    HeardCar h = { 0, 0, rain_roll_pitch(speed), followed && cockpit ? WET_ROLL_PAN_CENTRE : pan_of(race, car), speed };
    if (!(followed && cockpit && falling == RAIN_ROLL_RAIN)) h.spray = (rain_roll_spray(open, speed, ground) * level + 500) / 1000;
    if (ground == RAIN_ROLL_ASPHALT && open > 0 && rain_puddle_millihertz(speed) > 0)
        h.puddle = (int)((int64_t)RAIN_PUDDLE_PERCENT * level * (open > OPEN_FULL ? OPEN_FULL : open) / (1000 * OPEN_FULL));
    return h;
}

/* every car's spray on the loudest few voices, and their splashes */
static void cars_frame(const WeatherFrame *f, int falling, int followed_level)
{
    const RaceState *const race = f->race;
    const int32_t tick = (int32_t)race->tick;
    if (!cars.have || tick < cars.last_tick) cars_clear(), cars.have = 1, cars.last_tick = tick;
    const uint32_t ticks = (uint32_t)(tick - cars.last_tick);
    cars.last_tick = tick;
    HeardCar heard[RACE_CARS];
    int wanted[RACE_CARS], n = 0;
    const int count = race->car_count < RACE_CARS ? race->car_count : RACE_CARS;
    for (int i = 0; i < count; i++) {
        if (race->cars[i] == NULL) continue;
        heard[i] = heard_car(f, i, falling, followed_level);
        const int loud = heard[i].spray > heard[i].puddle ? heard[i].spray : heard[i].puddle;
        if (loud <= 0) continue;
        int k = n++;   /* the loudest first */
        for (; k > 0; k--) {
            const HeardCar *const o = &heard[wanted[k - 1]];
            if ((o->spray > o->puddle ? o->spray : o->puddle) >= loud) break;
            wanted[k] = wanted[k - 1];
        }
        wanted[k] = i;
    }
    int before[RAIN_CARS_VOICES];
    for (int v = 0; v < RAIN_CARS_VOICES; v++) before[v] = cars.voices.car[v];
    rain_car_voices_assign(&cars.voices, wanted, n, tick);
    for (int v = 0; v < RAIN_CARS_VOICES; v++)
        if (cars.voices.car[v] != before[v]) port_log("rain: tick %d spray voice %d car %d\n", (int)tick, v, cars.voices.car[v]);   /* who is heard, on a change only */
    int clips = -1;
    for (int v = 0; v < RAIN_CARS_VOICES; v++) {
        const int i = cars.voices.car[v];
        if (i < 0) {
            drive_audio_rain_spray(v, 0, 1000, WET_ROLL_PAN_CENTRE, -1);
            continue;
        }
        const HeardCar *const h = &heard[i];
        drive_audio_rain_spray(v, h->spray, h->pitch, h->pan, cars.voices.fresh[v] ? (int)(rain_dice(&cars.dice) % 1000) : -1);
        if (h->puddle <= 0 || ticks == 0) continue;
        if (clips < 0) clips = drive_audio_rain_puddle_clips();
        RainPuddle p;
        if (rain_puddle_roll(&cars.dice, h->speed, ticks, clips, cars.last_clip, &p)) {
            drive_audio_rain_puddle(p.clip, (h->puddle * p.gain_permille + 500) / 1000, p.pitch_permille, h->pan);
            cars.last_clip = p.clip;
        }
    }
}

/* port (rain_tyre_roll): what the car the camera follows hears of the rain, once a frame, in rain or a storm with rain_v2: the stopped layer (outside or in the cockpit, rain only:
 * falling snow is silent) giving way by its speed to the moving one (outside, the rain round a moving car; in the cockpit, the rain on the car with its own spray), by the openness
 * where the eye is (silent inside a closed zone); and every car's spray and splashes (cars_frame). The snowy rally's track is snow, not water: none of it there (snow_roll.h). The
 * voices fall silent by themselves when the frames stop, and at once in the pause and the in-race menu (drive_audio_weather_hush). */
void weather_hooks_rain_roll(const WeatherFrame *f)
{
    if (f->race != NULL) drive_audio_weather_wake((int32_t)f->race->tick);   /* back from the pause or the menu once the race moves */
    RainRoll r = rain_roll_levels(RAIN_ROLL_DRY, 0, 0, RAIN_ROLL_OFF, 0);
    const Car *const car = f->race != NULL ? f->race->cars[race_slot(f->race->viewed)] : NULL;
    if (car != NULL && port_rain_tyre_roll() && port_rain_v2() && weather_wet(f->weather) && f->race->port_rally_gravel != RALLY_GRAVEL_SNOW) {
        const int falling = f->snow_level != SNOW_NONE ? RAIN_ROLL_SNOW : RAIN_ROLL_RAIN;
        r = rain_roll_levels(falling, f->rain_open, car->speed, wet_ground(f->race, car), in_cockpit(f->race));
        cars_frame(f, falling, view_level(f->race, car));
    } else {
        cars_silent();
    }
    drive_audio_rain_roll(r.out, r.in, r.bed, r.move_in);
}
