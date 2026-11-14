/* the storm's thunder voices (thunder.h) */
#include "tests/test.h"

#include "game/audio/hush.h"
#include "game/audio/thunder.h"
#include "game/race/tick_rate.h"

#include <string.h>

enum { RATE = 22050, SAMPLE_FRAMES = 1000 };

static void put16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v) { put16(p, v), put16(p + 2, v >> 16); }

/* a WAV of `frames` mono 16-bit samples, every one `value` */
static size_t make_wav(uint8_t *out, uint32_t frames, int16_t value, uint32_t rate, uint32_t channels)
{
    const uint32_t data = frames * 2 * channels;
    memcpy(out, "RIFF", 4), put32(out + 4, 36 + data), memcpy(out + 8, "WAVEfmt ", 8), put32(out + 16, 16);
    put16(out + 20, 1), put16(out + 22, channels), put32(out + 24, rate), put32(out + 28, rate * 2 * channels), put16(out + 32, 2 * channels), put16(out + 34, 16);
    memcpy(out + 36, "data", 4), put32(out + 40, data);
    for (uint32_t i = 0; i < frames * channels; i++) put16(out + 44 + 2 * i, (uint16_t)value);
    return 44 + data;
}

static void test_load(void)
{
    static uint8_t wav[44 + 4 * SAMPLE_FRAMES];
    Thunder t;
    memset(&t, 0, sizeof t);
    CHECK_EQ_INT(thunder_load(&t, wav, make_wav(wav, SAMPLE_FRAMES, 1000, RATE, 1), RATE), 1);
    CHECK_EQ_INT((int)t.frames, SAMPLE_FRAMES);
    CHECK_EQ_INT(thunder_load(&t, wav, make_wav(wav, SAMPLE_FRAMES, 1000, 44100, 1), RATE), 0);   /* another rate */
    CHECK_EQ_INT(thunder_load(&t, wav, make_wav(wav, SAMPLE_FRAMES, 1000, RATE, 2), RATE), 0);    /* stereo */
    CHECK_EQ_INT(thunder_load(&t, (const uint8_t *)"RIFFxxxx", 8, RATE), 0);                      /* too short */
    CHECK_EQ_INT((int)t.frames, SAMPLE_FRAMES);                                                  /* a refusal leaves the sample */
    thunder_free(&t);
    CHECK(t.pcm == NULL);
}

static void test_play(void)
{
    static uint8_t wav[44 + 2 * SAMPLE_FRAMES];
    Thunder t;
    memset(&t, 0, sizeof t);
    thunder_schedule(&t, 10, 100);   /* no sample: nothing */
    CHECK_EQ_INT(thunder_active(&t), 0);
    CHECK_EQ_INT(thunder_load(&t, wav, make_wav(wav, SAMPLE_FRAMES, 10000, RATE, 1), RATE), 1);
    thunder_schedule(&t, 60, 100);   /* a second (60 race ticks): RATE frames of silence */
    CHECK_EQ_INT(thunder_active(&t), 1);
    static int16_t out[2 * RATE];
    memset(out, 0, sizeof out);
    thunder_mix(&t, out, RATE - 10);
    for (int i = 0; i < 2 * (RATE - 10); i++) CHECK_EQ_INT(out[i], 0);   /* nothing before the delay */
    memset(out, 0, sizeof out);
    thunder_mix(&t, out, 20);   /* the last 10 silent frames, then 10 of the sample */
    CHECK_EQ_INT(out[2 * 9], 0);
    CHECK_EQ_INT(out[2 * 10], 10000 * THUNDER_GAIN_PERCENT / 100);
    CHECK_EQ_INT(out[2 * 10 + 1], 10000 * THUNDER_GAIN_PERCENT / 100);   /* both channels */
    memset(out, 0, sizeof out);
    thunder_mix(&t, out, SAMPLE_FRAMES);   /* the rest, and it is over */
    CHECK_EQ_INT(thunder_active(&t), 0);
    thunder_free(&t);
}

static void test_volume_and_sum(void)
{
    static uint8_t wav[44 + 2 * SAMPLE_FRAMES];
    Thunder t;
    memset(&t, 0, sizeof t);
    thunder_load(&t, wav, make_wav(wav, SAMPLE_FRAMES, 20000, RATE, 1), RATE);
    thunder_schedule(&t, 0, 50);
    int16_t out[2 * 4] = { 0 };
    thunder_mix(&t, out, 4);
    CHECK_EQ_INT(out[0], 20000 * (50 * THUNDER_GAIN_PERCENT / 100) / 100);   /* quieter by its volume */
    thunder_schedule(&t, 0, 100), thunder_schedule(&t, 0, 100), thunder_schedule(&t, 0, 100);   /* louder together than a short can hold */
    int16_t loud[2 * 4];
    for (int i = 0; i < 8; i++) loud[i] = 30000;
    thunder_mix(&t, loud, 4);
    CHECK_EQ_INT(loud[0], 32767);   /* saturates, does not wrap */
    thunder_free(&t);
}

static void test_voices_replaced(void)
{
    static uint8_t wav[44 + 2 * SAMPLE_FRAMES];
    Thunder t;
    memset(&t, 0, sizeof t);
    thunder_load(&t, wav, make_wav(wav, SAMPLE_FRAMES, 100, RATE, 1), RATE);
    for (int i = 0; i < THUNDER_VOICES + 3; i++) thunder_schedule(&t, 30 + i, 100);
    CHECK_EQ_INT(thunder_active(&t), THUNDER_VOICES);   /* never more than the voices */
    thunder_free(&t);
}

/* the weather's hush (hush.h): whole, the clap as it was, to the sample; at half, half; quiet, the claps sounding stopped and the ones to come still to come */
static void test_hushed(void)
{
    static uint8_t wav[44 + 4 * SAMPLE_FRAMES];
    Thunder a, b;
    memset(&a, 0, sizeof a), memset(&b, 0, sizeof b);
    thunder_load(&a, wav, make_wav(wav, SAMPLE_FRAMES, 30000, RATE, 1), RATE);
    thunder_load(&b, wav, make_wav(wav, SAMPLE_FRAMES, 30000, RATE, 1), RATE);
    thunder_schedule(&a, 0, 100), thunder_schedule(&a, 0, 100);   /* two at once: over full scale, clipped as before */
    thunder_schedule(&b, 0, 100), thunder_schedule(&b, 0, 100);
    static int16_t plain[2 * 200], gained[2 * 200];
    static int32_t whole[200], half[200];
    for (int i = 0; i < 200; i++) whole[i] = HUSH_UNITY, half[i] = HUSH_UNITY / 2, plain[2 * i] = plain[2 * i + 1] = gained[2 * i] = gained[2 * i + 1] = (int16_t)(i * 50);
    thunder_mix(&a, plain, 100);
    thunder_mix_gained(&b, gained, 100, whole);
    CHECK(memcmp(plain, gained, sizeof plain) == 0);
    memset(gained, 0, sizeof gained);
    thunder_mix_gained(&b, gained, 1, half);
    CHECK_EQ_INT(gained[0], 2 * (30000 * 70 / 100 / 2));   /* each clap at half */
    thunder_schedule(&b, 3, 100);   /* one to come in 3 race ticks */
    CHECK(thunder_active(&b) == 3);
    thunder_quiet(&b);   /* the two sounding stop; the one to come waits on */
    CHECK_EQ_INT(thunder_active(&b), 1);
    memset(gained, 0, sizeof gained);
    thunder_mix(&b, gained, 200);
    CHECK(gained[0] == 0 && gained[399] == 0);   /* nothing until its time */
    static int16_t later[2 * RATE];
    thunder_mix(&b, later, RATE / 5);
    const int due = 3 * RATE / RACE_TICKS_A_SECOND - 200;   /* its delay less the 200 frames already mixed */
    CHECK(later[2 * (due - 1)] == 0 && later[2 * due] == 30000 * 70 / 100);   /* then the clap, whole, on time */
    thunder_free(&a), thunder_free(&b);
}

int main(void)
{
    test_load();
    test_play();
    test_volume_and_sum();
    test_voices_replaced();
    test_hushed();
    return TEST_REPORT();
}
