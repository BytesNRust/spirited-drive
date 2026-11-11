/* The weather's frame hooks (weather_hooks.h): the lightning's level, the wipers' pass in the box's pixels, and the blades' darkening table. */
#include "game/viewer/weather_hooks.h"
#include "game/platform/platform.h"
#include "game/race/weather.h"
#include "game/race/rain_roll.h"
#include "game/race/rally_gravel.h"
#include "game/race/snow_roll.h"
#include "game/race/weather_snow.h"
#include "tests/test.h"

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
static int wet_percent = -1, wet_pitch = -1, snow_percent = -1, snow_pitch = -1;
void drive_audio_thunder(int delay_ticks, int volume_percent)
{
    if (thunder_calls < 64) thunder_delay[thunder_calls] = delay_ticks, thunder_volume[thunder_calls] = volume_percent;
    thunder_calls++;
}
void drive_audio_wet_roll(int volume_percent, int pitch_permille) { wet_percent = volume_percent, wet_pitch = pitch_permille; }
void drive_audio_snow_roll(int volume_percent, int pitch_permille) { snow_percent = volume_percent, snow_pitch = pitch_permille; }

static WeatherFrame frame(int weather, int snow, int open)
{
    return (WeatherFrame){ weather, snow, open, NULL, 1000 };
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
    wet_percent = snow_percent = -1;
    weather_hooks_rain_roll(&f);
    weather_hooks_snow_roll(&f);
    CHECK_EQ_INT(wet_percent, 0);
    CHECK_EQ_INT(wet_pitch, RAIN_ROLL_PITCH_SLOW);
    CHECK_EQ_INT(snow_percent, 0);
    CHECK_EQ_INT(snow_pitch, SNOW_ROLL_PITCH_SLOW);
}

static void test_the_voices_follow_the_viewed_car(void)
{
    static RaceState race;
    static Car car;
    memset(&race, 0, sizeof race), memset(&car, 0, sizeof car);
    setenv("TNFS_RAIN_V2", "1", 1), setenv("TNFS_RAIN_TYRE_ROLL", "1", 1);
    race.cars[0] = &car, race.viewed = 0;
    car.speed = 40 << 16;
    WeatherFrame f = frame(WEATHER_RAIN, SNOW_NONE, 16);
    f.race = &race;
    weather_hooks_rain_roll(&f);
    CHECK_EQ_INT(wet_percent, rain_roll_volume(1, 16, car.speed, 1));   /* on asphalt */
    CHECK_EQ_INT(wet_pitch, rain_roll_pitch(car.speed));
    CHECK(wet_percent > 0);
    car.airborne_ticks = 3;
    weather_hooks_rain_roll(&f);
    CHECK_EQ_INT(wet_percent, rain_roll_volume(1, 16, car.speed, 0));   /* in the air: not on asphalt */
    car.airborne_ticks = 0, race.port_rally_gravel = RALLY_GRAVEL_COMPACT;
    weather_hooks_rain_roll(&f);
    CHECK_EQ_INT(wet_percent, rain_roll_volume(1, 16, car.speed, 0));   /* the rally's gravel */
    f.weather = WEATHER_CLEAR;
    weather_hooks_rain_roll(&f);
    CHECK_EQ_INT(wet_percent, 0);                                         /* dry: silent */
    CHECK_EQ_INT(wet_pitch, RAIN_ROLL_PITCH_SLOW);
    race.port_rally_gravel = RALLY_GRAVEL_SNOW;
    weather_hooks_snow_roll(&f);
    CHECK_EQ_INT(snow_percent, snow_roll_volume(RALLY_GRAVEL_SNOW, car.speed, 1));
    CHECK_EQ_INT(snow_pitch, snow_roll_pitch(car.speed));
    CHECK(snow_percent > 0);
    race.port_rally_gravel = RALLY_GRAVEL_DUST;
    weather_hooks_snow_roll(&f);
    CHECK_EQ_INT(snow_percent, 0);                                        /* not the snowy rally */
    CHECK_EQ_INT(snow_pitch, SNOW_ROLL_PITCH_SLOW);
}

int main(void)
{
    test_the_flash_is_the_storms_alone();
    test_the_wipers_pass();
    test_the_dark_table_follows_the_palette();
    test_the_thunder_follows_the_strikes();
    test_the_voices_are_silent_without_a_race();
    test_the_voices_follow_the_viewed_car();
    return TEST_REPORT();
}
