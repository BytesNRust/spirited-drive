/* The NITRO cheat's start keys (start_keys.h): the four key bytes filled from the key flags each step, the port's
 * rule (N alone, or the pad's nitro key) against the exe's (N and a shift or B), and the revved launch they open in
 * car_drive.c (launch). Replays and the race without the cheat leave them alone. */
#include "game/race/car_drive.h"
#include "game/race/race_keys.h"
#include "game/race/start_keys.h"
#include "test.h"

#include <string.h>

enum { KEY_FLAGS = 256, PORT_RULE = 1, EXE_RULE = 0, CHEAT = 1, NO_CHEAT = 0, PLAYING = 1, RACING = 0 };

static void press(uint8_t *keys, int code)
{
    keys[code] = 1;
}

static void test_exe_rule_needs_n_and_a_shift_or_b(void)
{
    uint8_t keys[KEY_FLAGS] = { 0 }, out[4] = { 0 };
    press(keys, START_KEY_N);
    start_keys_fill(out, keys, CHEAT, RACING, EXE_RULE);
    CHECK_EQ_INT(out[0], 1);
    CHECK_EQ_INT(out[1] | out[2] | out[3], 0);   /* N alone: the shifts and B are up */
    press(keys, START_KEY_LSHIFT);
    start_keys_fill(out, keys, CHEAT, RACING, EXE_RULE);
    CHECK_EQ_INT(out[0], 1);
    CHECK_EQ_INT(out[1], 1);
    CHECK_EQ_INT(out[2], 0);
    CHECK_EQ_INT(out[3], 0);
}

static void test_exe_rule_copies_the_other_flags(void)
{
    uint8_t keys[KEY_FLAGS] = { 0 }, out[4] = { 0 };
    press(keys, START_KEY_RSHIFT);
    press(keys, START_KEY_B);
    start_keys_fill(out, keys, CHEAT, RACING, EXE_RULE);
    CHECK_EQ_INT(out[0], 0);   /* no N: nothing */
    CHECK_EQ_INT(out[1], 0);
    CHECK_EQ_INT(out[2], 1);
    CHECK_EQ_INT(out[3], 1);
}

static void test_port_rule_takes_n_alone(void)
{
    uint8_t keys[KEY_FLAGS] = { 0 }, out[4] = { 0 };
    press(keys, START_KEY_N);
    start_keys_fill(out, keys, CHEAT, RACING, PORT_RULE);
    CHECK_EQ_INT(out[0], 1);
    CHECK_EQ_INT(out[1] | out[2] | out[3], 0);
}

static void test_port_rule_takes_the_pad_key_and_the_exe_rule_does_not(void)
{
    uint8_t keys[KEY_FLAGS] = { 0 }, out[4] = { 0 };
    press(keys, RACE_KEYS_PAD_NITRO);
    start_keys_fill(out, keys, CHEAT, RACING, PORT_RULE);
    CHECK_EQ_INT(out[0], 1);
    out[0] = 0;
    start_keys_fill(out, keys, CHEAT, RACING, EXE_RULE);
    CHECK_EQ_INT(out[0], 0);
}

static void test_nothing_held_fills_zeros(void)
{
    uint8_t keys[KEY_FLAGS] = { 0 }, out[4] = { 1, 1, 1, 1 };
    start_keys_fill(out, keys, CHEAT, RACING, PORT_RULE);
    CHECK_EQ_INT(out[0] | out[1] | out[2] | out[3], 0);
}

static void test_without_the_cheat_or_in_playback_nothing_is_written(void)
{
    uint8_t keys[KEY_FLAGS] = { 0 }, out[4] = { 9, 9, 9, 9 };
    press(keys, START_KEY_N);
    start_keys_fill(out, keys, NO_CHEAT, RACING, PORT_RULE);
    CHECK_EQ_INT(out[0], 9);
    start_keys_fill(out, keys, CHEAT, PLAYING, PORT_RULE);
    CHECK_EQ_INT(out[0], 9);
    CHECK_EQ_INT(out[3], 9);
}

/* The start clock (004bb8e0) is the input's clock while the cheat runs in a race, and is left alone otherwise. */
static void test_start_clock_follows_the_input_clock_under_the_cheat(void)
{
    int32_t clock = 7;
    start_clock_fill(&clock, 240, CHEAT, RACING);
    CHECK_EQ_INT(clock, 240);
    start_clock_fill(&clock, 260, NO_CHEAT, RACING);
    CHECK_EQ_INT(clock, 240);
    start_clock_fill(&clock, 280, CHEAT, PLAYING);
    CHECK_EQ_INT(clock, 240);
}

/* A car held in gear with the race's start: the launch opens when the keys are down before the lights. */
static void set_up_start(RaceState *race, Car *car)
{
    memset(race, 0, sizeof *race);
    memset(car, 0, sizeof *car);
    car->engaged = 1;
    race->start_input = CHEAT;
    race->start_clock = 150;   /* past LAUNCH_EARLIEST (100) */
    race->port_nitro_start = PORT_RULE;
}

static void test_launch_opens_on_n_alone_with_the_port_rule(void)
{
    static RaceState race;
    static Car car;
    static CarSpecs specs;
    uint8_t keys[KEY_FLAGS] = { 0 };
    set_up_start(&race, &car);
    press(keys, START_KEY_N);
    start_keys_fill(race.start_keys, keys, race.start_input, race.playback, race.port_nitro_start);
    car_drive_force(&car, &specs, &race);
    CHECK_EQ_INT(race.launch, 1);
    CHECK_EQ_INT(car.throttle, 0xff);
}

static void test_launch_needs_a_shift_or_b_with_the_exe_rule(void)
{
    static RaceState race;
    static Car car;
    static CarSpecs specs;
    uint8_t keys[KEY_FLAGS] = { 0 };
    set_up_start(&race, &car);
    race.port_nitro_start = EXE_RULE;
    press(keys, START_KEY_N);
    start_keys_fill(race.start_keys, keys, race.start_input, race.playback, race.port_nitro_start);
    car_drive_force(&car, &specs, &race);
    CHECK_EQ_INT(race.launch, 0);   /* N alone is not the exe's start */
    press(keys, START_KEY_RSHIFT);
    start_keys_fill(race.start_keys, keys, race.start_input, race.playback, race.port_nitro_start);
    car_drive_force(&car, &specs, &race);
    CHECK_EQ_INT(race.launch, 1);
    CHECK_EQ_INT(car.throttle, 0xff);
}

static void test_launch_from_the_pad_key_with_the_port_rule(void)
{
    static RaceState race;
    static Car car;
    static CarSpecs specs;
    uint8_t keys[KEY_FLAGS] = { 0 };
    set_up_start(&race, &car);
    press(keys, RACE_KEYS_PAD_NITRO);
    start_keys_fill(race.start_keys, keys, race.start_input, race.playback, race.port_nitro_start);
    car_drive_force(&car, &specs, &race);
    CHECK_EQ_INT(race.launch, 1);
}

static void test_no_launch_without_the_cheat_or_in_playback(void)
{
    static RaceState race;
    static Car car;
    static CarSpecs specs;
    uint8_t keys[KEY_FLAGS] = { 0 };
    set_up_start(&race, &car);
    race.start_input = NO_CHEAT;
    press(keys, START_KEY_N);
    start_keys_fill(race.start_keys, keys, race.start_input, race.playback, race.port_nitro_start);
    car_drive_force(&car, &specs, &race);
    CHECK_EQ_INT(race.launch, 0);
    race.start_input = CHEAT;
    race.playback = PLAYING;
    start_keys_fill(race.start_keys, keys, race.start_input, race.playback, race.port_nitro_start);
    car_drive_force(&car, &specs, &race);
    CHECK_EQ_INT(race.launch, 0);
}

int main(void)
{
    test_exe_rule_needs_n_and_a_shift_or_b();
    test_exe_rule_copies_the_other_flags();
    test_port_rule_takes_n_alone();
    test_port_rule_takes_the_pad_key_and_the_exe_rule_does_not();
    test_nothing_held_fills_zeros();
    test_without_the_cheat_or_in_playback_nothing_is_written();
    test_start_clock_follows_the_input_clock_under_the_cheat();
    test_launch_opens_on_n_alone_with_the_port_rule();
    test_launch_needs_a_shift_or_b_with_the_exe_rule();
    test_launch_from_the_pad_key_with_the_port_rule();
    test_no_launch_without_the_cheat_or_in_playback();
    return TEST_REPORT();
}
