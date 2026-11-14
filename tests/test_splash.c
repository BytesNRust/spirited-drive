/* the puddle splashes' one-shot voices (splash.h) */
#include "tests/test.h"

#include "game/audio/splash.h"
#include "game/audio/wet_roll.h"

#include <string.h>

enum { RATE = 22050, FRAMES = 400 };

static void put16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v) { put16(p, v), put16(p + 2, v >> 16); }

/* a mono 16-bit WAV of `frames` samples, all `value` */
static size_t make_wav(uint8_t *out, uint32_t frames, int16_t value, uint32_t rate)
{
    const uint32_t data = frames * 2;
    memcpy(out, "RIFF", 4), put32(out + 4, 36 + data), memcpy(out + 8, "WAVEfmt ", 8), put32(out + 16, 16);
    put16(out + 20, 1), put16(out + 22, 1), put32(out + 24, rate), put32(out + 28, rate * 2), put16(out + 32, 2), put16(out + 34, 16);
    memcpy(out + 36, "data", 4), put32(out + 40, data);
    for (uint32_t i = 0; i < frames; i++) put16(out + 44 + 2 * i, (uint16_t)value);
    return 44 + data;
}

static void test_plays_once_panned(void)
{
    static Splash s;
    static uint8_t wav[44 + 2 * FRAMES];
    memset(&s, 0, sizeof s);
    CHECK_EQ_INT(splash_load(&s, wav, make_wav(wav, FRAMES, 1000, RATE), RATE), 1);
    CHECK_EQ_INT(splash_load(&s, wav, make_wav(wav, FRAMES, 1000, 44100), RATE), 0);   /* another rate: refused */
    CHECK_EQ_INT(s.clips, 1);
    int16_t out[2 * 1000];
    memset(out, 0, sizeof out);
    splash_play(&s, 0, 100, 1000, WET_ROLL_PAN_CENTRE);
    CHECK_EQ_INT(splash_active(&s), 1);
    splash_mix(&s, out, 1000);
    CHECK(out[0] == 1000 && out[1] == 1000 && out[2 * (FRAMES - 2)] == 1000 && out[2 * FRAMES] == 0);   /* the clip once, at its level */
    CHECK_EQ_INT(splash_active(&s), 0);
    memset(out, 0, sizeof out);
    splash_play(&s, 0, 50, 1000, WET_ROLL_PAN_LEFT);   /* half, on the left alone */
    splash_mix(&s, out, 10);
    CHECK(out[0] == 500 && out[1] == 0);
    splash_play(&s, 0, 100, 2000, WET_ROLL_PAN_RIGHT);   /* twice the rate: half as long */
    memset(out, 0, sizeof out);
    splash_mix(&s, out, 1000);
    CHECK(out[2 * (FRAMES / 2 - 2) + 1] == 1000 && out[2 * (FRAMES / 2 + 2) + 1] == 0);
    splash_play(&s, 3, 100, 1000, WET_ROLL_PAN_CENTRE);   /* a clip not loaded, a level of 0: nothing */
    splash_play(&s, 0, 0, 1000, WET_ROLL_PAN_CENTRE);
    CHECK_EQ_INT(splash_active(&s), 0);
    for (int i = 0; i < SPLASH_VOICES + 3; i++) splash_play(&s, 0, 100, 1000, WET_ROLL_PAN_CENTRE);   /* more than the voices: the oldest give way */
    CHECK_EQ_INT(splash_active(&s), SPLASH_VOICES);
    splash_free(&s);
    CHECK_EQ_INT(s.clips, 0);
}

/* the loop voice's pan (the driver's law), its shared sample and its seek */
static void test_loop_pan_share_seek(void)
{
    CHECK(wet_roll_pan_gain(WET_ROLL_PAN_CENTRE, 0) == 0x7f && wet_roll_pan_gain(WET_ROLL_PAN_CENTRE, 1) == 0x7f);
    CHECK(wet_roll_pan_gain(WET_ROLL_PAN_LEFT, 1) == 0 && wet_roll_pan_gain(WET_ROLL_PAN_LEFT, 0) == 0x7f);
    CHECK(wet_roll_pan_gain(WET_ROLL_PAN_RIGHT, 0) == 0 && wet_roll_pan_gain(WET_ROLL_PAN_RIGHT, 1) == 0x7f);
    static uint8_t wav[44 + 2 * FRAMES];
    WetRoll owner, voice;
    CHECK_EQ_INT(wet_roll_load(&owner, wav, make_wav(wav, FRAMES, 1000, RATE), RATE), 1);
    wet_roll_share(&voice, &owner.sample);
    wet_roll_seek(&voice, 500);
    CHECK_EQ_INT((int)(voice.position >> 16), FRAMES / 2);
    wet_roll_set(&voice, 100, 1000);
    wet_roll_pan(&voice, WET_ROLL_PAN_RIGHT);
    int16_t out[2 * 4000];
    memset(out, 0, sizeof out);
    wet_roll_mix(&voice, out, 4000);   /* eased in, panned over a tenth of a second */
    CHECK(out[2 * 3999] == 0 && out[2 * 3999 + 1] == 1000);
    CHECK(out[2 * 10] < 100 && out[2 * 10] >= 0);   /* no click */
    wet_roll_free(&voice);   /* the shared sample stays its owner's */
    CHECK(owner.sample.pcm != NULL);
    wet_roll_free(&owner);
}

int main(void)
{
    test_plays_once_panned();
    test_loop_pan_share_seek();
    return TEST_REPORT();
}
