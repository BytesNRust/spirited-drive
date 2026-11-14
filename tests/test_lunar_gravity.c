/* Lunar gravity (track_gravity.h, lunar_springs.h's lunar_gravity_active): the moon's race pulls a third as hard and
 * its surfaces grip at two thirds of the rally's; the exe's numbers stay when the bit is clear. */
#include "game/race/lunar_springs.h"
#include "game/race/race_state.h"
#include "game/race/track_gravity.h"
#include "game/sys/port_extras.h"
#include "game/sys/port_profile.h"
#include "test.h"

#include "game/platform/platform.h"

#include <string.h>

/* port_extras' file: none here (the defaults and the parse only) */
uint8_t *platform_read_user_file(const char *name, size_t *size) { (void)name, (void)size; return NULL; }
bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    (void)name, (void)data, (void)size;
    return false;
}
void platform_free_file(uint8_t *data) { (void)data; }

static void test_pull_is_a_third_only_on_the_moon(void)
{
    CHECK_EQ_INT(track_gravity(0, 900), 900);
    CHECK_EQ_INT(track_gravity(1, 900), 300);
}

static void test_grip_is_two_thirds_only_on_the_moon(void)
{
    CHECK_EQ_INT(track_grip(0, 0x180), 0x180);
    CHECK_EQ_INT(track_grip(1, 0x180), 0x100);
}

static void test_rally_surfaces_take_the_moon_grip(void)
{
    RaceState race;
    memset(&race, 0, sizeof race);
    race.track = LUNAR_TRACK_RUSTY;
    race.arcade = 1;
    race.track_flags = TRACK_FLAG_LOW_GRAVITY;   /* the exe's own bit changes nothing */
    race_surfaces_init(&race);
    CHECK_EQ_INT(race.surfaces[1].grip, 0x180);
    race.port_lunar_gravity = 1;
    race_surfaces_init(&race);
    for (int i = 0; i < RACE_SURFACES; i++) CHECK_EQ_INT(race.surfaces[i].grip, 0x100);
}

static void test_gravity_switch_needs_the_moons_race(void)
{
    CHECK_EQ_INT(lunar_gravity_active(LUNAR_TRACK_RUSTY, 1, 1, 1), 1);
    CHECK_EQ_INT(lunar_gravity_active(LUNAR_TRACK_RUSTY, 1, 1, 0), 0);   /* lunar_gravity off */
    CHECK_EQ_INT(lunar_gravity_active(LUNAR_TRACK_RUSTY, 1, 0, 1), 0);   /* lunar_springs off */
    CHECK_EQ_INT(lunar_gravity_active(LUNAR_TRACK_RUSTY, 0, 1, 1), 0);   /* not the rally */
    CHECK_EQ_INT(lunar_gravity_active(0, 1, 1, 1), 0);                   /* another track */
}

/* rain_grip: the wet road grips at four fifths; the shoulder keeps its own; the flag off is the exe's */
static void test_rain_grip_is_four_fifths_of_the_road_only(void)
{
    CHECK_EQ_INT(track_rain_grip(0, 0x100), 0x100);
    CHECK_EQ_INT(track_rain_grip(1, 0x100), 0xcc);
    RaceState race;
    memset(&race, 0, sizeof race);
    race.track = 1;
    race_surfaces_init(&race);
    const int32_t dry_road = race.surfaces[0].grip, dry_shoulder = race.surfaces[1].grip;
    race.port_rain_grip = 1;
    race_surfaces_init(&race);
    CHECK_EQ_INT(race.surfaces[0].grip, dry_road * RAIN_GRIP_NUM / RAIN_GRIP_DEN);
    CHECK_EQ_INT(race.surfaces[1].grip, dry_shoulder);   /* the shoulder is loose already */
    race.port_rain_grip = 0;
    race_surfaces_init(&race);
    CHECK_EQ_INT(race.surfaces[0].grip, dry_road);
}

/* the three states: Off nowhere, Lunar Springs the moon's race alone (Rusty Springs, the rally, the Lunar Springs Track
 * switch), All Tracks every race whatever the track, the variant or the switch */
static void test_the_three_states(void)
{
    for (int track = 0; track < 9; track++)
        for (int rally = 0; rally < 2; rally++)
            for (int lunar = 0; lunar < 2; lunar++) {
                const int moon = track == LUNAR_TRACK_RUSTY && rally && lunar;
                CHECK_EQ_INT(lunar_gravity_active(track, rally, lunar, LUNAR_GRAVITY_OFF), 0);
                CHECK_EQ_INT(lunar_gravity_active(track, rally, lunar, LUNAR_GRAVITY_SPRINGS), moon);
                CHECK_EQ_INT(lunar_gravity_active(track, rally, lunar, LUNAR_GRAVITY_ALL), 1);
            }
}

/* Off in every mode until the player changes it */
static void test_off_in_every_mode(void)
{
    for (int p = 0; p < PROFILE_COUNT; p++)
        CHECK_EQ_INT(port_profile_extras((Profile)p, port_extras_defaults()).lunar_gravity, LUNAR_GRAVITY_OFF);
}

/* the file: an older 0/1 keeps its meaning (1 the moon's race alone), 2 every track, nothing else read */
static void test_the_file_keeps_the_meaning(void)
{
    CHECK_EQ_INT(port_extras_parse("lunar_gravity=1\n", 16).lunar_gravity, LUNAR_GRAVITY_SPRINGS);
    CHECK_EQ_INT(port_extras_parse("lunar_gravity=2\n", 16).lunar_gravity, LUNAR_GRAVITY_ALL);
    CHECK_EQ_INT(port_extras_parse("lunar_gravity=3\n", 16).lunar_gravity, LUNAR_GRAVITY_OFF);
    PortExtras e = port_extras_defaults();
    e.lunar_gravity = LUNAR_GRAVITY_ALL;
    char text[1024];
    const size_t n = port_extras_format(&e, text, sizeof text);
    CHECK(n > 0);
    CHECK_EQ_INT(port_extras_parse(text, n).lunar_gravity, LUNAR_GRAVITY_ALL);
}

int main(void)
{
    test_the_three_states();
    test_off_in_every_mode();
    test_the_file_keeps_the_meaning();
    test_rain_grip_is_four_fifths_of_the_road_only();
    test_pull_is_a_third_only_on_the_moon();
    test_grip_is_two_thirds_only_on_the_moon();
    test_rally_surfaces_take_the_moon_grip();
    test_gravity_switch_needs_the_moons_race();
    return TEST_REPORT();
}
