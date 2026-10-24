/* port_pad_and_keys (input_read.c): the keys and the pad read together. Flag 0 is the exe's: the device words choose.
 *   test_pad_and_keys */
#include "game/race/input_read.h"

#include <stdio.h>
#include <string.h>

enum { LEFT = 0x4b, RIGHT = 0x4d, UP = 0x48, DOWN = 0x50, GEAR_UP_KEY = 0x1e, GEAR_DOWN_KEY = 0x2c, HAND_KEY = 0x39,
       HORN_KEY = 0x23 };
enum { CENTRE = 0x20, FULL = 0x3c, AT_REST = 0x1e, PRESSED = 5 };

static int failures;
#define CHECK_EQ(got, want)                                                                                       \
    do {                                                                                                          \
        const long g_ = (long)(got), w_ = (long)(want);                                                           \
        if (g_ != w_) {                                                                                           \
            fprintf(stderr, "%s:%d: %s = %ld, want %ld\n", __FILE__, __LINE__, #got, g_, w_);                     \
            failures++;                                                                                           \
        }                                                                                                         \
    } while (0)

static const int32_t CODES[INPUT_KEYS] = { LEFT, RIGHT, UP, DOWN, GEAR_UP_KEY, GEAR_DOWN_KEY, HAND_KEY, HORN_KEY };
static InputPad the_pad;

static void pad_read(void *ctx, int32_t player, int16_t stand_in)
{
    (void)player, (void)stand_in;
    *(InputPad *)ctx = the_pad;
}

/* the word with `down` keys held, the pad as `x`, `y`, `buttons` say, devices all the keyboard (0) or all the pad */
static uint32_t word(int flag, int pad_present, int x, int y, int buttons, int analog, const int *down, int n)
{
    static ControlSetup controls[RACE_PLAYERS];
    static RaceEntry entries[RACE_PLAYERS];
    int16_t stand_in[RACE_PLAYERS] = { 0 };
    uint8_t keys[256] = { 0 };
    for (int i = 0; i < n; i++) keys[down[i]] = 1;
    the_pad = (InputPad){ x, y, (uint8_t)buttons };
    controls[0].analog_steering = analog;
    controls[0].pedals = analog;
    controls[0].gears = analog ? 3 : 0;
    InputPad pad = { 0, 0, 0 };
    MouseSteerWords mouse = { 0 };
    const InputReadOps ops = { &pad, pad_read, { 0 } };
    const InputReadInputs in = { keys, CODES, stand_in, controls, entries, 0, 0, pad_present, flag };
    return input_read(&ops, &pad, &mouse, &in);
}

#define STEER(w) ((w) & 0x3f)
#define PEDAL(w) (((w) >> 6) & 0x3f)
#define GEARS(w) (((w) >> 12) & 3)

static void test_keyboard_only(void)
{
    const int left[] = { LEFT }, right[] = { RIGHT }, gas[] = { UP }, brake[] = { DOWN, UP }, up[] = { GEAR_UP_KEY },
              down[] = { GEAR_DOWN_KEY };
    for (int pad_present = 0; pad_present <= 1; pad_present++) {
        CHECK_EQ(STEER(word(1, pad_present, 0, 0, 0, 0, left, 1)), 0);
        CHECK_EQ(STEER(word(1, pad_present, 0, 0, 0, 0, right, 1)), FULL);
        CHECK_EQ(STEER(word(1, pad_present, 0, 0, 0, 0, NULL, 0)), CENTRE);
        CHECK_EQ(PEDAL(word(1, pad_present, 0, 0, 0, 0, gas, 1)), PRESSED);
        CHECK_EQ(PEDAL(word(1, pad_present, 0, 0, 0, 0, brake, 2)), FULL);
        CHECK_EQ(PEDAL(word(1, pad_present, 0, 0, 0, 0, NULL, 0)), AT_REST);
        CHECK_EQ(GEARS(word(1, pad_present, 0, 0, 0, 0, up, 1)), 1);
        CHECK_EQ(GEARS(word(1, pad_present, 0, 0, 0, 0, down, 1)), 2);
    }
    /* the same as the exe's read with the keyboard's words, whatever the flag */
    for (int k = 0; k < 2; k++) {
        const int *keys = k ? left : right;
        CHECK_EQ(word(1, 0, 0, 0, 0, 0, keys, 1), word(0, 0, 0, 0, 0, 0, keys, 1));
    }
}

static void test_pad_only(void)
{
    CHECK_EQ(STEER(word(1, 1, -0x20, 0, 0, 0, NULL, 0)), 0);
    CHECK_EQ(STEER(word(1, 1, 0x1f, 0, 0, 0, NULL, 0)), 0x3f);
    CHECK_EQ(STEER(word(1, 1, 8, 0, 0, 0, NULL, 0)), CENTRE + 8);
    CHECK_EQ(PEDAL(word(1, 1, 0, -0x1e, 0, 0, NULL, 0)), 0);       /* the trigger all the way: full gas */
    CHECK_EQ(PEDAL(word(1, 1, 0, 0x1e, 0, 0, NULL, 0)), FULL);     /* the brake all the way */
    CHECK_EQ(PEDAL(word(1, 1, 0, -10, 0, 0, NULL, 0)), AT_REST - 10);
    CHECK_EQ(GEARS(word(1, 1, 0, 0, 1, 0, NULL, 0)), 1);
    CHECK_EQ(GEARS(word(1, 1, 0, 0, 2, 0, NULL, 0)), 2);
    CHECK_EQ((word(1, 1, 0, 0, 8, 0, NULL, 0) >> 12) & 8, 8);     /* the handbrake */
    /* the device words are not looked at: the keyboard's, with the stick, steers the same */
    CHECK_EQ(word(1, 1, 8, -10, 1, 0, NULL, 0), word(1, 1, 8, -10, 1, 3, NULL, 0));
}

static void test_both_at_once(void)
{
    const int left[] = { LEFT }, brake[] = { DOWN }, gas[] = { UP };
    /* the stick off centre wins over the keys; centred, the keys steer */
    CHECK_EQ(STEER(word(1, 1, 8, 0, 0, 0, left, 1)), CENTRE + 8);
    CHECK_EQ(STEER(word(1, 1, -12, 0, 0, 0, (const int[]){ RIGHT }, 1)), CENTRE - 12);
    CHECK_EQ(STEER(word(1, 1, 0, 0, 0, 0, left, 1)), 0);
    /* a key's brake with the trigger's gas, each its own; both at once: the further from the rest wins */
    CHECK_EQ(PEDAL(word(1, 1, 0, 0, 0, 0, brake, 1)), FULL);
    CHECK_EQ(PEDAL(word(1, 1, 0, -0x1e, 0, 0, NULL, 0)), 0);
    CHECK_EQ(PEDAL(word(1, 1, 0, -10, 0, 0, brake, 1)), FULL);
    CHECK_EQ(PEDAL(word(1, 1, 0, 0x1e, 0, 0, gas, 1)), FULL);
    CHECK_EQ(PEDAL(word(1, 1, 0, -0x1e, 0, 0, gas, 1)), 0);
    CHECK_EQ(PEDAL(word(1, 1, 0, 0, 0, 0, gas, 1)), PRESSED);
    /* a gear key and a pad button */
    CHECK_EQ(GEARS(word(1, 1, 0, 0, 2, 0, (const int[]){ GEAR_UP_KEY }, 1)), 1);
    /* the horn is its key alone: the pad's button 0 is a gear, not a horn */
    CHECK_EQ((word(1, 1, 0, 0, 1, 0, NULL, 0) >> 12) & 4, 0);
    CHECK_EQ((word(1, 1, 0, 0, 0, 0, (const int[]){ HORN_KEY }, 1) >> 12) & 4, 4);
}

static void test_flag_zero_is_the_exe(void)
{
    const int left[] = { LEFT };
    /* keyboard words: the stick is not read */
    CHECK_EQ(STEER(word(0, 1, 8, 0, 0, 0, left, 1)), 0);
    CHECK_EQ(PEDAL(word(0, 1, 0, -0x1e, 0, 0, NULL, 0)), AT_REST);
    /* pad words: the keys are not read, and button 0 honks */
    CHECK_EQ(STEER(word(0, 1, 8, 0, 0, 1, left, 1)), CENTRE + 8);
    CHECK_EQ(STEER(word(0, 1, 0, 0, 0, 1, left, 1)), CENTRE);
    CHECK_EQ(PEDAL(word(0, 1, 0, -0x1e, 0, 1, NULL, 0)), 0);
}

int main(void)
{
    test_keyboard_only();
    test_pad_only();
    test_both_at_once();
    test_flag_zero_is_the_exe();
    if (failures == 0) printf("test_pad_and_keys: ok\n");
    return failures != 0;
}
