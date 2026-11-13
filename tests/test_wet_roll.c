/* the wet-tyre roll's voice (wet_roll.h) */
#include "tests/test.h"

#include "game/audio/wet_roll.h"

#include <string.h>

enum { RATE = 22050, SAMPLE_FRAMES = 1000 };

static void put16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v) { put16(p, v), put16(p + 2, v >> 16); }

/* a mono 16-bit WAV of `frames` samples, a ramp 0, 10, 20 ... so a position can be read back from the sound */
static size_t make_wav(uint8_t *out, uint32_t frames)
{
    const uint32_t data = frames * 2;
    memcpy(out, "RIFF", 4), put32(out + 4, 36 + data), memcpy(out + 8, "WAVEfmt ", 8), put32(out + 16, 16);
    put16(out + 20, 1), put16(out + 22, 1), put32(out + 24, RATE), put32(out + 28, RATE * 2), put16(out + 32, 2), put16(out + 34, 16);
    memcpy(out + 36, "data", 4), put32(out + 40, data);
    for (uint32_t i = 0; i < frames; i++) put16(out + 44 + 2 * i, (uint16_t)(i * 10));
    return 44 + data;
}

static void load(WetRoll *w)
{
    static uint8_t wav[44 + 2 * SAMPLE_FRAMES];
    CHECK_EQ_INT(wet_roll_load(w, wav, make_wav(wav, SAMPLE_FRAMES), RATE), 1);
}

static void test_silent_adds_nothing(void)
{
    WetRoll w;
    load(&w);
    int16_t out[2 * 64];
    memset(out, 0, sizeof out);
    wet_roll_mix(&w, out, 64);   /* a level of 0 (nothing set) */
    int any = 0;
    for (size_t i = 0; i < sizeof out / sizeof *out; i++) any |= out[i] != 0;
    CHECK_EQ_INT(any, 0);
    CHECK_EQ_INT((int)w.position, 0);   /* and the loop did not move */
    WetRoll none;
    memset(&none, 0, sizeof none);
    wet_roll_set(&none, 50, 1000);
    wet_roll_mix(&none, out, 64);   /* no sample: nothing, no crash */
    CHECK_EQ_INT(out[10], 0);
    wet_roll_free(&w);
}

static void test_level_eases_and_loops(void)
{
    WetRoll w;
    load(&w);
    wet_roll_set(&w, 100, 1000);
    int16_t out[2 * 4000];
    memset(out, 0, sizeof out);
    wet_roll_mix(&w, out, 4000);
    CHECK(out[0] == 0 && out[2 * 3] < 100);                  /* eased in: not the full level at once */
    CHECK_EQ_INT(w.level, 100 * 256);                        /* and at the target within a fraction of a second */
    CHECK_EQ_INT(out[2 * 1500], out[2 * 1500 + 1]);          /* both channels */
    CHECK_EQ_INT(out[2 * 2500 + 0], (int16_t)(((2500 % SAMPLE_FRAMES)) * 10));   /* the loop wrapped: frame 2500 is sample 500 of the ramp, at full level */
    CHECK(w.position < (uint64_t)SAMPLE_FRAMES << 16);
    wet_roll_set(&w, 0, 1000);
    wet_roll_mix(&w, out, 4000);
    CHECK_EQ_INT(w.level, 0);   /* eased out */
    wet_roll_free(&w);
}

static void test_stale_falls_silent(void)
{
    WetRoll w;
    load(&w);
    wet_roll_set(&w, 100, 1000);
    int16_t out[2 * RATE];
    memset(out, 0, sizeof out);
    wet_roll_mix(&w, out, RATE / 4);
    CHECK_EQ_INT(w.target, 100 * 256);   /* a quarter second on: still asked for */
    wet_roll_mix(&w, out, RATE);         /* nobody set it again: silent past half a second */
    CHECK_EQ_INT(w.target, 0);
    CHECK_EQ_INT(w.level, 0);
    wet_roll_set(&w, 50, 1000);          /* set again: it plays again */
    CHECK_EQ_INT(w.target, 50 * 256);
    wet_roll_free(&w);
}

static void test_pitch_and_saturation(void)
{
    WetRoll w;
    load(&w);
    wet_roll_set(&w, 100, 2000);   /* twice the rate: two sample frames a frame */
    int16_t out[2 * 3000];
    memset(out, 0, sizeof out);
    wet_roll_mix(&w, out, 3000);
    CHECK_EQ_INT(out[2 * 2000], (int16_t)(((2 * 2000) % SAMPLE_FRAMES) * 10));
    wet_roll_set(&w, 100, 1000);
    for (size_t i = 0; i < sizeof out / sizeof *out; i++) out[i] = 32767;   /* over a loud mix: saturates, does not wrap */
    wet_roll_mix(&w, out, 3000);
    int wrapped = 0;
    for (size_t i = 0; i < sizeof out / sizeof *out; i++) wrapped |= out[i] < 0;
    CHECK_EQ_INT(wrapped, 0);
    wet_roll_set(&w, 280, 1000);   /* above 100 lifts a quiet sample */
    CHECK_EQ_INT(w.target, 280 * 256);
    wet_roll_set(&w, WET_ROLL_MAX_PERCENT + 100, 50);   /* clamped: the level at the cap, the pitch at its floor */
    CHECK(w.target == WET_ROLL_MAX_PERCENT * 256 && w.pitch >= 100);
    wet_roll_free(&w);
}

int main(void)
{
    test_silent_adds_nothing();
    test_level_eases_and_loops();
    test_stale_falls_silent();
    test_pitch_and_saturation();
    return TEST_REPORT();
}
