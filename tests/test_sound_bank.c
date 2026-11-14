/* A sound bank's samples kept within its file (sound_bank.h): a mod's bank (fuzz/sound_bank.c) whose header gave more frames than the
 * file has made the mixer read past it; the frames and the loop are cut to the bytes there, and a sample with none is not registered */
#include "tests/test.h"

#include "game/audio/sound_bank.h"

#include <string.h>

enum { TABLE = 4 * SOUND_BANK_SOUNDS, RECORD_AT = TABLE, HEADER_AT = RECORD_AT + SOUND_BANK_RECORD, DATA_AT = HEADER_AT + SOUND_BANK_HEADER };

static SoundDriver driver;
static SoundBank bank;
static uint8_t file[DATA_AT + 16384];

static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8), p[2] = (uint8_t)(v >> 16), p[3] = (uint8_t)(v >> 24); }

/* sound 1: a record, its header (`size` 1 or 2 bytes a sample, `channels`, `compression`, `type`), `data` bytes after it */
static size_t one_sound(uint8_t size, uint8_t channels, uint8_t compression, int8_t type, uint32_t frames, uint32_t loop_start,
                        uint32_t loop_frames, size_t data)
{
    memset(file, 0, sizeof file);
    put32(file + 4, RECORD_AT);
    put32(file + RECORD_AT + 4, HEADER_AT);
    uint8_t *h = file + HEADER_AT;
    memcpy(h, "EACS", 4), put32(h + 4, 22050);
    h[8] = size, h[9] = channels, h[0xa] = compression, h[0xb] = (uint8_t)type;
    put32(h + 0xc, frames), put32(h + 0x10, loop_start), put32(h + 0x14, loop_frames), put32(h + 0x18, DATA_AT);
    return DATA_AT + data;
}

static const SoundSample *registered(size_t size)
{
    memset(&driver, 0, sizeof driver), memset(&bank, 0, sizeof bank);
    sound_driver_wire(&driver);
    sound_channels_reset(&driver.channels);
    driver.channels.open = 1;
    if (sound_bank_register(&driver, &bank, file, size) < 0 || driver.registry[1] == NULL) return NULL;
    return &bank.sample[1];
}

/* a header that fits is read as it is */
static void test_fitting_kept(void)
{
    const SoundSample *s = registered(one_sound(2, 2, 0, -1, 1000, 40, 900, 4000));
    CHECK(s != NULL);
    CHECK_EQ_INT((int)s->frames, 1000);
    CHECK_EQ_INT((int)s->loop_start, 40);
    CHECK_EQ_INT((int)s->loop_frames, 900);
}

/* more frames than bytes: cut to the bytes, the loop with them */
static void test_frames_cut(void)
{
    const SoundSample *s = registered(one_sound(2, 1, 0, -1, 100000, 10, 99000, 400));   /* 16-bit mono: 200 frames there */
    CHECK(s != NULL);
    CHECK_EQ_INT((int)s->frames, 200);
    CHECK_EQ_INT((int)s->loop_start, 10);
    CHECK_EQ_INT((int)s->loop_frames, 190);
    s = registered(one_sound(1, 1, 0, -1, 500, 300, 50, 100));   /* 8-bit: the loop starts past the 100 frames there: none */
    CHECK(s != NULL);
    CHECK_EQ_INT((int)s->frames, 100);
    CHECK_EQ_INT((int)s->loop_frames, 0);
    s = registered(one_sound(1, 1, 2, -1, 5000, 0, 0, 100));   /* compressed mono: 4 bits a frame, 200 there */
    CHECK(s != NULL);
    CHECK_EQ_INT((int)s->frames, 200);
}

/* no frames, or a compressed block whose own count runs past the file: not registered */
static void test_nothing_to_play(void)
{
    CHECK(registered(one_sound(1, 1, 0, -1, 0, 0, 0, 100)) == NULL);
    CHECK(registered(one_sound(2, 1, 0, -1, 10, 0, 0, 1)) == NULL);   /* not one whole frame */
    const size_t size = one_sound(1, 1, 2, 3, 10, 0, 0, 0x14 + 8);
    put32(file + DATA_AT, 1000);   /* the block says 1000 samples, 16 are there */
    CHECK(registered(size) == NULL);
    put32(file + DATA_AT, 16);
    CHECK(registered(size) != NULL);
}

/* a sample whose type (+0xb, the channel it names) is past the 16: played on the channel it was given, not read past the channels */
static void test_type_past_channels(void)
{
    SoundSample *s = (SoundSample *)registered(one_sound(2, 1, 0, 122, 100, 0, 0, 200));
    CHECK(s != NULL);
    if (s == NULL) return;
    sound_driver_init(&driver, 0x5622);
    CHECK(sound_play_on(&driver, s, 2, 0x40, 0x7f) >= 0);
    CHECK(driver.voice[2].on);
}

/* a compressed block (a type) with a loop plays its bytes as they are: its frames and loop cut to them as a plain sample's */
static void test_looped_block_cut(void)
{
    const size_t size = one_sound(1, 1, 2, 0, 1359893, 1063424, 295168, 0x14 + 200);
    put32(file + DATA_AT, 16);   /* the block's count fits */
    const SoundSample *s = registered(size);
    CHECK(s != NULL);
    if (s == NULL) return;
    CHECK_EQ_INT((int)s->frames, 0x14 + 200);   /* 8-bit mono: a byte a frame */
    CHECK_EQ_INT((int)s->loop_frames, 0);       /* its loop starts past them */
    sound_driver_init(&driver, 0x5622);
    CHECK(sound_play_on(&driver, (SoundSample *)s, 2, 0x40, 0x7f) >= 0);
    static int16_t out[2 * 512];
    for (int i = 0; i < 4; i++) (void)sound_mix_frames(&driver.mixer, out, 512);
}

/* a compressed sample whose rate (+4) is 0xff000000: each mixed frame steps over hundreds of its frames, more than the decode
 * buffer holds in one span; the span is held to what the buffer holds, the mixer not read past it */
static void test_rate_past_the_buffer(void)
{
    const size_t size = one_sound(2, 2, 2, -1, 12021, 4, 12017, 12100);
    put32(file + HEADER_AT + 4, 0xff000000u);
    for (int i = 0; i < 12100; i++) file[DATA_AT + i] = (uint8_t)(i * 7);
    SoundSample *s = (SoundSample *)registered(size);
    CHECK(s != NULL);
    if (s == NULL) return;
    sound_driver_init(&driver, 0x5622);
    CHECK(sound_play_on(&driver, s, 2, 0x40, 0x7f) >= 0);
    static int16_t out[2 * 512];
    for (int i = 0; i < 4; i++) (void)sound_mix_frames(&driver.mixer, out, 512);
}

int main(void)
{
    test_rate_past_the_buffer();
    test_looped_block_cut();
    test_type_past_channels();
    test_fitting_kept();
    test_frames_cut();
    test_nothing_to_play();
    return TEST_REPORT();
}
