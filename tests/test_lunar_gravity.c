/* Lunar gravity (track_gravity.h, lunar_springs.h's lunar_gravity_active): the moon's race pulls a third as hard and
 * its surfaces grip at two thirds of the rally's; the exe's numbers stay when the bit is clear. */
#include "game/race/lunar_springs.h"
#include "game/race/race_state.h"
#include "game/race/track_gravity.h"
#include "test.h"

#include <string.h>

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

int main(void)
{
    test_rain_grip_is_four_fifths_of_the_road_only();
    test_pull_is_a_third_only_on_the_moon();
    test_grip_is_two_thirds_only_on_the_moon();
    test_rally_surfaces_take_the_moon_grip();
    test_gravity_switch_needs_the_moons_race();
    return TEST_REPORT();
}
