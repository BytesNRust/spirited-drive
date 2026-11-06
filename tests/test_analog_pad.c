/* analog_pad.h: which control words are a pad's deflection and which a key's, and how input_read.c writes them. */
#include "game/race/analog_pad.h"
#include "game/race/input_read.h"
#include "test.h"

#include <stdint.h>

static uint32_t word(uint32_t steer, uint32_t pedal) { return steer | pedal << 6; }

static void test_keys_are_never_analog(void)
{
    const uint32_t rest = word(0x20, 0x1e), left = word(0, 0x1e), right = word(0x3c, 0x1e), gas = word(0x20, 5), brake = word(0x20, 0x3c);
    CHECK_EQ_INT(analog_pad_steering(1, rest) | analog_pad_steering(1, left) | analog_pad_steering(1, right), 0);   /* centre, key left, key right */
    CHECK_EQ_INT(analog_pad_pedal(1, rest) | analog_pad_pedal(1, gas) | analog_pad_pedal(1, brake), 0);            /* rest, the accelerate key, the brake key */
}

static void test_a_pads_partial_axes_are_analog(void)
{
    CHECK(analog_pad_pedal(1, word(0x20, 0x0f)));    /* a trigger at about half: p = 0x1e less 15 */
    CHECK(analog_pad_pedal(1, word(0x20, 0x26)));    /* the brake trigger at about a fifth */
    CHECK(analog_pad_pedal(1, word(0x20, 0)));       /* the accelerate trigger to the end (the pad writes 0, the key 5) */
    CHECK(analog_pad_steering(1, word(0x28, 0x1e))); /* the stick at about a quarter right */
    CHECK(analog_pad_steering(1, word(0x10, 0x1e))); /* and half left */
    CHECK(analog_pad_steering(1, word(0x3f, 0x1e))); /* a wheel's full range axis to its end (the key writes 0x3c) */
}

enum { LEFT = 0x4b, RIGHT = 0x4d, UP = 0x48, DOWN = 0x50 };
static const int32_t CODES[INPUT_KEYS] = { LEFT, RIGHT, UP, DOWN, 0x1e, 0x2c, 0x39, 0x23 };
static InputPad the_pad;
static void pad_read(void *ctx, int32_t player, int16_t stand_in) { (void)player, (void)stand_in; *(InputPad *)ctx = the_pad; }

/* the word input_read.c makes with port_pad_and_keys, the pad's stick `x`, trigger pedal `y` and `key` held (0 none) */
static uint32_t read_word(int x, int y, int key)
{
    static ControlSetup controls[RACE_PLAYERS];
    static RaceEntry entries[RACE_PLAYERS];
    int16_t stand_in[RACE_PLAYERS] = { 0 };
    uint8_t keys[256] = { 0 };
    if (key) keys[key] = 1;
    the_pad = (InputPad){ x, y, 0 };
    InputPad pad = { 0, 0, 0 };
    MouseSteerWords mouse = { 0 };
    const InputReadOps ops = { &pad, pad_read, { 0 } };
    const InputReadInputs in = { keys, CODES, stand_in, controls, entries, 0, 0, 1, 1 };
    return input_read(&ops, &pad, &mouse, &in);
}

static void test_the_words_input_read_makes(void)
{
    CHECK(analog_pad_pedal(1, read_word(0, -15, 0)));         /* half a trigger: a word between the key values */
    CHECK_EQ_INT(((read_word(0, -15, 0) >> 6) & 0x3f), 0x0f);
    CHECK(analog_pad_pedal(1, read_word(0, 10, 0)));          /* a third of the brake trigger */
    CHECK(analog_pad_steering(1, read_word(8, 0, 0)));        /* a quarter of the stick */
    CHECK_EQ_INT(analog_pad_pedal(1, read_word(0, 0, UP)), 0);   /* the accelerate key alone: the key's word */
    CHECK_EQ_INT(analog_pad_pedal(1, read_word(0, 0, DOWN)), 0);
    CHECK_EQ_INT(analog_pad_steering(1, read_word(0, 0, LEFT)) | analog_pad_steering(1, read_word(0, 0, RIGHT)), 0);
    CHECK_EQ_INT(analog_pad_pedal(1, read_word(0, 0, 0)) | analog_pad_steering(1, read_word(0, 0, 0)), 0);   /* nothing held: the rest */
    CHECK_EQ_INT(analog_pad_pedal(1, read_word(0, -5, UP)), 0);  /* a key further than the trigger wins and stays the key's */
    CHECK(analog_pad_pedal(1, read_word(0, -28, UP)));           /* a trigger further than the key wins, analog */
}

static void test_off_is_the_exes_paths(void)
{
    CHECK_EQ_INT(analog_pad_pedal(0, word(0x20, 0x0f)), 0);
    CHECK_EQ_INT(analog_pad_steering(0, word(0x28, 0x1e)), 0);
}

int main(void)
{
    test_keys_are_never_analog();
    test_a_pads_partial_axes_are_analog();
    test_the_words_input_read_makes();
    test_off_is_the_exes_paths();
    return TEST_REPORT();
}
