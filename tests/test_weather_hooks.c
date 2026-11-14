/* The weather's frame hooks (weather_hooks.h): the lightning's level, the wipers' pass in the box's pixels, and the blades' darkening table. */
#include "game/viewer/weather_hooks.h"
#include "game/platform/platform.h"
#include "game/race/tick_rate.h"
#include "game/race/weather.h"
#include "game/race/rain_cars.h"
#include "game/race/rain_roll.h"
#include "game/audio/wet_roll.h"
#include "game/race/rally_gravel.h"
#include "game/race/snow_roll.h"
#include "game/race/weather_snow.h"
#include "game/viewer/drive_bumper.h"
#include "tests/test.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* (port_settings reads the switches' file: no file is read or written here) */
uint8_t *platform_read_user_file(const char *name, size_t *size) { (void)name, (void)size; return NULL; }
bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    (void)name, (void)data, (void)size;
    return false;
}
void platform_free_file(uint8_t *data) { free(data); }

/* the audio the hooks drive is the exe's (drive_audio.c): recorded here */
static int thunder_calls, thunder_delay[64], thunder_volume[64];
static int out_percent = -1, in_percent = -1, bed_percent = -1, move_in_percent = -1, snow_percent = -1, snow_pitch = -1;
static int spray_percent[RAIN_CARS_VOICES], spray_pitch[RAIN_CARS_VOICES], spray_pan[RAIN_CARS_VOICES], spray_starts, puddles, puddle_level[1024], puddle_pan[1024];
void drive_audio_thunder(int delay_ticks, int volume_percent)
{
    if (thunder_calls < 64) thunder_delay[thunder_calls] = delay_ticks, thunder_volume[thunder_calls] = volume_percent;
    thunder_calls++;
}
void drive_audio_rain_roll(int out, int in, int bed, int move_in) { out_percent = out, in_percent = in, bed_percent = bed, move_in_percent = move_in; }
void drive_audio_rain_spray(int voice, int percent, int pitch_permille, int pan, int start_permille)
{
    CHECK(voice >= 0 && voice < RAIN_CARS_VOICES && start_permille >= -1 && start_permille < 1000);
    spray_percent[voice] = percent, spray_pitch[voice] = pitch_permille, spray_pan[voice] = pan, spray_starts += start_permille >= 0;
}
int drive_audio_rain_puddle_clips(void) { return 7; }
void drive_audio_rain_puddle(int clip, int percent, int pitch_permille, int pan)
{
    CHECK(clip >= 0 && clip < 7 && percent > 0 && pitch_permille >= 950 && pitch_permille <= 1050);
    if (puddles < 1024) puddle_level[puddles] = percent, puddle_pan[puddles] = pan;
    puddles++;
}
/* the voices heard: their levels summed (the followed car alone: its voice's) */
static int sprays(void)
{
    int sum = 0;
    for (int v = 0; v < RAIN_CARS_VOICES; v++) sum += spray_percent[v];
    return sum;
}
static int voices_on(void)
{
    int n = 0;
    for (int v = 0; v < RAIN_CARS_VOICES; v++) n += spray_percent[v] > 0;
    return n;
}
void drive_audio_snow_roll(int volume_percent, int pitch_permille) { snow_percent = volume_percent, snow_pitch = pitch_permille; }

static WeatherFrame frame(int weather, int snow, int open)
{
    return (WeatherFrame){ weather, snow, open, NULL, 1000, NULL };
}

static void test_the_flash_is_the_storms_alone(void)
{
    int lit = 0;
    for (int tick = 0; tick < 3000; tick++) {
        WeatherFrame f = frame(WEATHER_STORM, SNOW_NONE, 16);
        f.anim_tick = tick;
        const int want = weather_flash_open(weather_rain_flash((uint32_t)tick), 16);
        CHECK_EQ_INT(weather_hooks_flash_level(&f), want);
        lit += want > 0;
        f.rain_open = 0;
        CHECK_EQ_INT(weather_hooks_flash_level(&f), weather_flash_open(weather_rain_flash((uint32_t)tick), 0));   /* a closed zone's own openness */
        f.snow_level = SNOW_LIGHT;
        CHECK_EQ_INT(weather_hooks_flash_level(&f), 0);                                                           /* a snowstorm has no lightning */
        f = frame(WEATHER_RAIN, SNOW_NONE, 16);
        f.anim_tick = tick;
        CHECK_EQ_INT(weather_hooks_flash_level(&f), 0);
    }
    CHECK(lit > 0);   /* the storm does flash in those ticks, or the test proves nothing */
}

static void test_the_wipers_pass(void)
{
    WiperPass pass;
    const int32_t box[4] = { 10, 20, 650, 500 };
    const WiperBlade blades[2] = { { 100, 400, 60, 0.1f, 1.2f, 1, 0, 0, 1, 300 }, { 200, 410, 70, 0.1f, 1.2f, 1, 0, 0, 1, 0 } };
    WeatherFrame dry = frame(WEATHER_CLEAR, SNOW_NONE, 16), rain = frame(WEATHER_RAIN, SNOW_NONE, 16), storm = frame(WEATHER_STORM, SNOW_NONE, 16), snow = frame(WEATHER_RAIN, SNOW_LIGHT, 16);
    ported_view_set_wipers(NULL, 0);
    CHECK_EQ_INT(weather_hooks_wiper_blades(), 0);
    CHECK(weather_hooks_wiper_pass(&pass, box, &rain) == NULL);   /* no blades set */
    ported_view_set_wipers(blades, 2);
    CHECK_EQ_INT(weather_hooks_wiper_blades(), 2);
    CHECK(weather_hooks_wiper_pass(&pass, box, &dry) == NULL);    /* nothing falls */
    CHECK(weather_hooks_wiper_pass(&pass, box, &rain) == &pass);
    CHECK_EQ_INT(pass.count, 2);
    CHECK(pass.blade[0].x == 90.0f && pass.blade[0].y == 380.0f);   /* in the box's own pixels */
    CHECK(pass.blade[0].xmax == 290.0f);                              /* a limit is moved too */
    CHECK(pass.blade[1].xmax == 0.0f);                                /* none stays none */
    const WiperTiming t_rain = pass.timing;
    CHECK(weather_hooks_wiper_pass(&pass, box, &storm) == &pass);
    CHECK(pass.timing.period == wiper_timing(WIPER_STORM).period && t_rain.period == wiper_timing(WIPER_RAIN).period);
    CHECK(weather_hooks_wiper_pass(&pass, box, &snow) == &pass);
    CHECK(pass.timing.period == wiper_timing(WIPER_SNOW).period);
    static const uint8_t glass[4] = { 1, 0, 0, 1 };
    ported_view_set_wiper_glass(glass, 2, 5.0f, 6.0f, 2.0f);
    CHECK(weather_hooks_wiper_pass(&pass, box, &rain) == &pass);
    CHECK(pass.glass == glass && pass.glass_x == -5.0f && pass.glass_y == -14.0f && pass.glass_scale == 2.0f && pass.glass_w == 2);
    ported_view_set_wiper_glass(NULL, 0, 0, 0, 0);
    CHECK(weather_hooks_wiper_pass(&pass, box, &rain) == &pass);
    CHECK(pass.glass == NULL && pass.glass_scale == 1.0f);   /* a scale that is not above 0 is 1 */
    ported_view_set_wipers(NULL, 0);
}

static void test_the_dark_table_follows_the_palette(void)
{
    uint8_t palette[768];
    for (int i = 0; i < 768; i++) palette[i] = (uint8_t)(i * 7);
    const uint8_t *a = weather_hooks_wiper_dark(palette);
    uint8_t first[256];
    memcpy(first, a, 256);
    CHECK(weather_hooks_wiper_dark(palette) == a);   /* the same table while the palette is the same */
    CHECK(memcmp(weather_hooks_wiper_dark(palette), first, 256) == 0);
    for (int i = 0; i < 768; i++) palette[i] = (uint8_t)(255 - i * 3);
    const uint8_t *b = weather_hooks_wiper_dark(palette);
    CHECK(memcmp(b, first, 256) != 0);               /* a new palette makes a new table */
}

static void test_the_thunder_follows_the_strikes(void)
{
    setenv("TNFS_RAIN_V2", "1", 1);
    int strikes = 0;
    thunder_calls = 0;
    WeatherFrame f = frame(WEATHER_STORM, SNOW_NONE, 16);
    f.anim_tick = 5000;
    weather_hooks_thunder(&f);   /* the first call starts afresh: no claps for ticks nobody saw */
    CHECK_EQ_INT(thunder_calls, 0);
    for (int tick = 5001; tick <= 5000 + 3000; tick++) {
        f.anim_tick = tick;
        const int before = thunder_calls;
        weather_hooks_thunder(&f);
        if (weather_thunder_strike((uint32_t)tick)) {
            const int delay = weather_thunder_delay((uint32_t)tick);
            CHECK_EQ_INT(thunder_calls, before + 1);
            if (before < 64) {
                CHECK_EQ_INT(thunder_delay[before], delay);
                CHECK_EQ_INT(thunder_volume[before], weather_thunder_inside(weather_thunder_volume(delay), 16));
            }
            strikes++;
        } else CHECK_EQ_INT(thunder_calls, before);
    }
    CHECK(strikes > 0);
    /* a jump of more than a few seconds starts afresh; so does the weather going dry in between */
    const int calls = thunder_calls;
    f.anim_tick += 5000;
    weather_hooks_thunder(&f);
    CHECK_EQ_INT(thunder_calls, calls);
    WeatherFrame dry = frame(WEATHER_RAIN, SNOW_NONE, 16);
    dry.anim_tick = f.anim_tick + 1;
    weather_hooks_thunder(&dry);
    f.anim_tick += 100;
    weather_hooks_thunder(&f);   /* afresh again: nothing played for the ticks between */
    CHECK_EQ_INT(thunder_calls, calls);
    WeatherFrame snowstorm = frame(WEATHER_STORM, SNOW_HEAVY, 16);
    snowstorm.anim_tick = f.anim_tick + 1;
    for (int i = 0; i < 400; i++, snowstorm.anim_tick++) weather_hooks_thunder(&snowstorm);   /* a snowstorm has no thunder */
    CHECK_EQ_INT(thunder_calls, calls);
}

static void test_the_voices_are_silent_without_a_race(void)
{
    setenv("TNFS_RAIN_TYRE_ROLL", "1", 1);
    const WeatherFrame f = frame(WEATHER_RAIN, SNOW_NONE, 16);   /* no race: no car to follow */
    out_percent = in_percent = bed_percent = move_in_percent = snow_percent = -1;
    for (int v = 0; v < RAIN_CARS_VOICES; v++) spray_percent[v] = -1;
    weather_hooks_rain_roll(&f);
    weather_hooks_snow_roll(&f);
    CHECK(out_percent == 0 && in_percent == 0 && bed_percent == 0 && move_in_percent == 0 && sprays() == 0);
    CHECK_EQ_INT(snow_percent, 0);
    CHECK_EQ_INT(snow_pitch, SNOW_ROLL_PITCH_SLOW);
}

/* a change of view in a race (the view key): the followed car's spray glides to the new view's level by the race's ticks, from the near chase camera to the far one and back; in
 * the cockpit it is in the cockpit's own layer. The eye's real distance counts (a camera still gliding to its place is between the two levels) */
static void test_the_wet_road_follows_the_view_live(void)
{
    static RaceState race;
    static Car car;
    memset(&race, 0, sizeof race), memset(&car, 0, sizeof car);
    setenv("TNFS_RAIN_V2", "1", 1), setenv("TNFS_RAIN_TYRE_ROLL", "1", 1);
    race.cars[0] = &car, race.car_count = 1, race.viewed = 0, race.tick = 1000;
    car.speed = 40 << 16, car.x = 100 << 16, car.z = 200 << 16;
    WeatherFrame f = frame(WEATHER_RAIN, SNOW_NONE, 16);
    f.race = &race;
    const int full = rain_roll_spray(16, car.speed, RAIN_ROLL_ASPHALT);
    #define AT(kind_, metres) (race.chase.kind = (kind_), race.chase.position[0] = car.x, race.chase.position[1] = car.y, race.chase.position[2] = car.z - (int32_t)((metres) * 65536.0))
    AT(2, 4.35);   /* the near chase camera: the race starts there, no glide */
    weather_hooks_rain_roll(&f);
    CHECK_EQ_INT(sprays(), full);
    CHECK_EQ_INT(spray_pitch[0], rain_roll_pitch(car.speed));
    CHECK(abs(spray_pan[0] - WET_ROLL_PAN_CENTRE) <= 1);   /* the car straight ahead of the eye */
    AT(3, 6.8);    /* the far one: down towards -6 dB, not at once */
    race.tick += 1;
    weather_hooks_rain_roll(&f);
    CHECK(sprays() < full && sprays() > full * 9 / 10);
    for (int i = 0; i < RACE_TICKS_A_SECOND; i++) race.tick++, weather_hooks_rain_roll(&f);
    CHECK(abs(sprays() - full * RAIN_ROLL_VIEW_FAR_PERMILLE / 1000) <= 1);
    weather_hooks_rain_roll(&f);   /* frames with no new tick: it holds */
    CHECK(abs(sprays() - full * RAIN_ROLL_VIEW_FAR_PERMILLE / 1000) <= 1);
    AT(DRIVE_DASH_KIND, 1.1);   /* the dash: the car's own spray is in the cockpit's layer */
    race.tick += RACE_TICKS_A_SECOND;
    weather_hooks_rain_roll(&f);
    CHECK(sprays() == 0 && move_in_percent == rain_roll_levels(RAIN_ROLL_RAIN, 16, car.speed, RAIN_ROLL_ASPHALT, 1).move_in && move_in_percent > 0 && bed_percent == 0);
    AT(1, 0.9);    /* the bumper: 0 dB */
    race.tick += RACE_TICKS_A_SECOND;
    weather_hooks_rain_roll(&f);
    CHECK(sprays() == full && move_in_percent == 0 && bed_percent == RAIN_ROLL_BED_PERCENT);
    AT(3, 5.5);    /* a camera on its way out: between the two */
    race.tick += RACE_TICKS_A_SECOND;
    weather_hooks_rain_roll(&f);
    CHECK(sprays() < full && sprays() > full * RAIN_ROLL_VIEW_FAR_PERMILLE / 1000);
    race.tick = 10;   /* a new race (the clock gone back): the level is the view's at once */
    AT(3, 6.8);
    weather_hooks_rain_roll(&f);
    CHECK(abs(sprays() - full * RAIN_ROLL_VIEW_FAR_PERMILLE / 1000) <= 1);
    #undef AT
}

static void test_the_voices_follow_the_viewed_car(void)
{
    static RaceState race;
    static Car car;
    memset(&race, 0, sizeof race), memset(&car, 0, sizeof car);
    setenv("TNFS_RAIN_V2", "1", 1), setenv("TNFS_RAIN_TYRE_ROLL", "1", 1);
    race.cars[0] = &car, race.car_count = 1, race.viewed = 0;
    race.chase.kind = 1;   /* a view outside the car, the eye on it */
    car.speed = 40 << 16;
    WeatherFrame f = frame(WEATHER_RAIN, SNOW_NONE, 16);
    f.race = &race;
    weather_hooks_rain_roll(&f);
    RainRoll r = rain_roll_levels(RAIN_ROLL_RAIN, 16, car.speed, RAIN_ROLL_ASPHALT, 0);   /* on asphalt, outside */
    CHECK(out_percent == r.out && in_percent == r.in && bed_percent == r.bed && move_in_percent == 0 && sprays() == r.spray && sprays() > 0);
    race.chase.kind = DRIVE_DASH_KIND;   /* the cockpit */
    race.tick += RACE_TICKS_A_SECOND;
    weather_hooks_rain_roll(&f);
    CHECK(move_in_percent == rain_roll_levels(RAIN_ROLL_RAIN, 16, car.speed, RAIN_ROLL_ASPHALT, 1).move_in && sprays() == 0 && bed_percent == 0);
    car.speed = 0;   /* stopped in the cockpit: the rain on the roof */
    weather_hooks_rain_roll(&f);
    CHECK(in_percent == RAIN_ROLL_IN_PERCENT && out_percent == 0 && move_in_percent == 0 && sprays() == 0);
    race.chase.kind = 1, race.tick += RACE_TICKS_A_SECOND;   /* stopped outside: the rain round the car (and the view's level glided back) */
    weather_hooks_rain_roll(&f);
    CHECK(out_percent == RAIN_ROLL_OUT_PERCENT && in_percent == 0 && bed_percent == 0);
    car.speed = 40 << 16, car.airborne_ticks = 3;
    weather_hooks_rain_roll(&f);
    CHECK(sprays() == 0 && bed_percent == RAIN_ROLL_BED_PERCENT);   /* in the air: no spray, the rain still round the car */
    car.airborne_ticks = 0, race.port_rally_gravel = RALLY_GRAVEL_COMPACT;
    weather_hooks_rain_roll(&f);
    CHECK(sprays() == rain_roll_spray(16, car.speed, RAIN_ROLL_GRAVEL) && sprays() > 0);   /* the rally's gravel: under the gravel roll */
    race.surfaces[0].off_road = 1;   /* the rally's road counts by the gravel roll's rule, not the surface's */
    weather_hooks_rain_roll(&f);
    CHECK_EQ_INT(sprays(), rain_roll_spray(16, car.speed, RAIN_ROLL_GRAVEL));
    car.node_flag_4aa = 1;   /* the rally's shoulder: the shoulder's own sound */
    weather_hooks_rain_roll(&f);
    CHECK_EQ_INT(sprays(), 0);
    car.node_flag_4aa = 0, race.surfaces[0].off_road = 0;
    race.port_rally_gravel = 0;
    f.weather = WEATHER_CLEAR;
    weather_hooks_rain_roll(&f);
    CHECK(out_percent == 0 && in_percent == 0 && bed_percent == 0 && move_in_percent == 0 && sprays() == 0);   /* dry: silent */
    f.weather = WEATHER_RAIN;
    setenv("TNFS_RAIN_TYRE_ROLL", "0", 1);   /* the switch off (Stock, Restored): silent, live */
    weather_hooks_rain_roll(&f);
    CHECK(out_percent == 0 && bed_percent == 0 && sprays() == 0);
    setenv("TNFS_RAIN_TYRE_ROLL", "1", 1);
    weather_hooks_rain_roll(&f);
    CHECK(bed_percent > 0 && sprays() > 0);
    /* snow falling: silent standing, the spray moving (it melts), in the cockpit too; no roll on snow off the snowy rally */
    f.snow_level = SNOW_HEAVY;
    car.speed = 0;
    weather_hooks_rain_roll(&f);
    weather_hooks_snow_roll(&f);
    CHECK(out_percent == 0 && in_percent == 0 && sprays() == 0 && snow_percent == 0);
    car.speed = 40 << 16;
    weather_hooks_rain_roll(&f);
    weather_hooks_snow_roll(&f);
    CHECK(sprays() == rain_roll_spray(16, car.speed, RAIN_ROLL_ASPHALT) && sprays() > 0 && out_percent == 0 && bed_percent == 0 && snow_percent == 0);
    race.chase.kind = DRIVE_DASH_KIND, race.tick += RACE_TICKS_A_SECOND;
    weather_hooks_rain_roll(&f);
    CHECK(sprays() == (rain_roll_spray(16, car.speed, RAIN_ROLL_ASPHALT) * RAIN_ROLL_VIEW_COCKPIT_PERMILLE + 500) / 1000 && move_in_percent == 0);
    race.chase.kind = 1, race.tick += RACE_TICKS_A_SECOND;
    race.port_rally_gravel = RALLY_GRAVEL_COMPACT;   /* snow falling on a rally: as rain, under the gravel roll */
    weather_hooks_rain_roll(&f);
    CHECK_EQ_INT(sprays(), rain_roll_spray(16, car.speed, RAIN_ROLL_GRAVEL));
    /* the snowy rally's track: the roll on snow alone, no spray */
    race.port_rally_gravel = RALLY_GRAVEL_SNOW;
    weather_hooks_rain_roll(&f);
    weather_hooks_snow_roll(&f);
    CHECK(sprays() == 0 && out_percent == 0 && snow_percent == snow_roll_volume(1, 16, car.speed, 1, 0) && snow_percent > 0);
    CHECK_EQ_INT(snow_pitch, snow_roll_pitch(car.speed));
    f.rain_open = 0;   /* a closed zone where snow falls */
    weather_hooks_snow_roll(&f);
    CHECK_EQ_INT(snow_percent, 0);
    f.snow_level = SNOW_NONE, f.rain_open = 16, f.weather = WEATHER_CLEAR;   /* nothing falling: the track is still snow */
    weather_hooks_snow_roll(&f);
    CHECK(snow_percent == snow_roll_volume(1, 16, car.speed, 1, 0) && snow_percent > 0);
    race.chase.kind = DRIVE_DASH_KIND;
    weather_hooks_snow_roll(&f);
    CHECK_EQ_INT(snow_percent, snow_roll_volume(1, 16, car.speed, 1, 1));
    race.port_rally_gravel = RALLY_GRAVEL_DUST;
    weather_hooks_snow_roll(&f);
    CHECK_EQ_INT(snow_percent, 0);   /* not the snowy rally */
    CHECK_EQ_INT(snow_pitch, SNOW_ROLL_PITCH_SLOW);
}

/* the road's openness by node: node 7 is in a tunnel */
static int open_at(int32_t node) { return node == 7 ? 0 : 16; }

/* the other cars (rivals, traffic): each its spray where it is, by its pace (how far it really goes), quieter the farther, on the side it is; none in a tunnel; the loudest
 * RAIN_CARS_VOICES at once; splashes at random, more the faster, none slow */
static void test_every_car_sprays(void)
{
    static RaceState race;
    static Car car[RACE_CARS];
    memset(&race, 0, sizeof race), memset(car, 0, sizeof car);
    setenv("TNFS_RAIN_V2", "1", 1), setenv("TNFS_RAIN_TYRE_ROLL", "1", 1);
    race.car_count = 2, race.viewed = 0, race.tick = 5000, race.chase.kind = 2;
    for (int i = 0; i < RACE_CARS; i++) race.cars[i] = &car[i], car[i].speed = 40 << 16, car[i].z = 0;
    car[1].x = 20 << 16;   /* 20 m to the side of the eye */
    race.chase.position[2] = -(int32_t)(4.35 * 65536);
    WeatherFrame f = frame(WEATHER_RAIN, SNOW_NONE, 16);
    f.race = &race, f.open_at = open_at;
    #define DRIVE(ticks) for (int t_ = 0; t_ < (ticks); t_++) { race.tick++; for (int i_ = 0; i_ < RACE_CARS; i_++) car[i_].z += car[i_].speed / RACE_TICKS_A_SECOND; race.chase.position[2] += car[0].speed / RACE_TICKS_A_SECOND; weather_hooks_rain_roll(&f); }
    weather_hooks_rain_roll(&f);
    CHECK_EQ_INT(voices_on(), 1);   /* the rival not seen going yet: standing */
    DRIVE(40);
    CHECK_EQ_INT(voices_on(), 2);
    const int32_t d = (int32_t)sqrt(pow(20.0, 2) + pow(4.35, 2)) * 65536;
    int rival = -1, own = -1;
    for (int v = 0; v < RAIN_CARS_VOICES; v++) {
        if (spray_percent[v] == rain_roll_spray(16, car[0].speed, RAIN_ROLL_ASPHALT)) own = v;
        else if (spray_percent[v] > 0) rival = v;
    }
    CHECK(own >= 0 && rival >= 0);
    if (rival >= 0) {
        const int want = rain_roll_spray(16, car[1].speed, RAIN_ROLL_ASPHALT) * rain_roll_view_permille(0, d) / 1000;
        CHECK(abs(spray_percent[rival] - want) <= want / 20 + 2 && spray_percent[rival] < spray_percent[own]);
        CHECK(spray_pan[rival] != WET_ROLL_PAN_CENTRE && abs(spray_pan[rival] - WET_ROLL_PAN_CENTRE) > 20);   /* off to one side */
    }
    CHECK(spray_starts >= 2);   /* each voice taken started somewhere in its loop */
    car[1].node = 7;   /* the rival in a tunnel: silent */
    DRIVE(30);
    CHECK_EQ_INT(voices_on(), 1);
    car[1].node = 0;
    /* nine cars near: the loudest four */
    race.car_count = RACE_CARS;
    for (int i = 1; i < RACE_CARS; i++) car[i].x = (i * 3) << 16, car[i].z = car[0].z;
    DRIVE(60);
    CHECK_EQ_INT(voices_on(), RAIN_CARS_VOICES);
    /* splashes: about one a second at full speed per car heard, none slow */
    for (int i = 0; i < RACE_CARS; i++) car[i].speed = RAIN_ROLL_FULL;
    DRIVE(30);
    puddles = 0;
    DRIVE(RACE_TICKS_A_SECOND * 20);
    CHECK(puddles > 20 * RAIN_CARS_VOICES / 2 && puddles < 20 * RAIN_CARS_VOICES * 2);
    for (int i = 0; i < RACE_CARS; i++) car[i].speed = RAIN_PUDDLE_FROM;
    DRIVE(30);
    puddles = 0;
    DRIVE(RACE_TICKS_A_SECOND * 10);
    CHECK_EQ_INT(puddles, 0);
    #undef DRIVE
}

int main(void)
{
    test_the_flash_is_the_storms_alone();
    test_the_wipers_pass();
    test_the_dark_table_follows_the_palette();
    test_the_thunder_follows_the_strikes();
    test_the_voices_are_silent_without_a_race();
    test_the_voices_follow_the_viewed_car();
    test_the_wet_road_follows_the_view_live();
    test_every_car_sprays();
    return TEST_REPORT();
}
