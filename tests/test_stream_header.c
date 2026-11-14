/* The music stream's header (sound_driver.h): its sample is started on the channel its +0x1c byte names, a byte the file gives; a mod's
 * stream (fuzz/music_stream.c) with 155 there made the start read channel 155 of 16. A byte past the channels is the stream's own
 * channel now; the CD's streams have 0 there, their channel, and start as before. */
#include "tests/test.h"

#include "game/audio/sound_driver.h"

#include <stdlib.h>
#include <string.h>

enum { BASE = 0x10000000, BLOCK = BASE + 0x40, HEADER = BLOCK + 8, CHANNEL = 3 };

enum { RING = 0x400 };

static SoundDriver d;
static uint8_t *ring;   /* on the heap, as the game's, so a read past it shows */

static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8), p[2] = (uint8_t)(v >> 16), p[3] = (uint8_t)(v >> 24); }

/* a '1SNh' block of 0x100 bytes: its EACS header (16-bit mono, compressed when `packed`, the stream's channel at +0xb, `named` at
 * +0x1c), silence after */
static void start_at(uint32_t at, uint32_t size, uint32_t samples, uint8_t named, int packed, int channels, uint32_t rate)
{
    memset(&d, 0, sizeof d), memset(ring, 0, RING);
    sound_driver_wire(&d);
    sound_channels_reset(&d.channels);
    d.channels.open = 1;
    sound_driver_init(&d, 0x5622);
    uint8_t *b = ring + (at - BASE);
    memcpy(b, "1SNh", 4), put32(b + 4, size);
    uint8_t *h = b + 8;
    memcpy(h, "EACS", 4), put32(h + 4, rate);
    put32(b + 0x28, samples);   /* the first data's (+0x28) */
    h[8] = 2, h[9] = (uint8_t)channels, h[0xa] = packed ? 2 : 0, h[0xb] = CHANNEL, h[0x1c] = named, h[0x1d] = 0x7f;
    d.stream.ring = (SoundRing){ ring, BASE, RING };
    sound_stream_reset(&d.stream, 1);
    CHECK_EQ_INT(sound_stream_block(&d, at), 0);
    CHECK(sound_stream_start(&d) >= 0);
}

static void start_stream(uint8_t named, int packed, int channels, uint32_t rate) { start_at(BLOCK, 0x100, 0, named, packed, channels, rate); }
static void start_packed(uint8_t named, int packed) { start_stream(named, packed, 1, 22050); }
static void start(uint8_t named) { start_packed(named, 0); }

/* a compressed '1SNd' block whose step index (+4) is 546 of the 89 steps: read as the last step, not past the table */
static void test_packed_index(void)
{
    start_packed(0, 1);
    enum { DATA = BLOCK + 0x100 };
    uint8_t *b = ring + (DATA - BASE);
    memcpy(b, "1SNd", 4), put32(b + 4, 0x100);
    put32(b + 8, 64);    /* the samples */
    put32(b + 12, 546);  /* the left channel's step index */
    for (int i = 0; i < 32; i++) b[8 + 0x14 + i] = 0x77;
    CHECK_EQ_INT(sound_stream_block(&d, DATA), 0);
    static int16_t out[2 * 256];
    for (int i = 0; i < 4; i++) (void)sound_mix_frames(&d.mixer, out, 256);
    CHECK(1);
}

/* a compressed block that says 100000 samples in its 0x100 bytes: decoded to the block's end, not past it (and the ring) */
static void test_packed_count(void)
{
    start_packed(0, 1);
    enum { DATA = BLOCK + 0x100 };
    uint8_t *b = ring + (DATA - BASE);
    memcpy(b, "1SNd", 4), put32(b + 4, 0x100);
    put32(b + 8, 100000);
    CHECK_EQ_INT(sound_stream_block(&d, DATA), 0);
    static int16_t out[2 * 256];
    for (int i = 0; i < 8; i++) (void)sound_mix_frames(&d.mixer, out, 256);
    CHECK(1);
}

/* a compressed stereo block that ends where the ring does, played at a rate with a fraction: its last span decodes a frame or two
 * past the block (the fraction's carry, as the original), into the zero bytes the ring has after it (SOUND_RING_PAD) */
static void test_packed_ring_end(void)
{
    start_stream(0, 1, 2, 22000);
    enum { DATA = BASE + RING - 0x100 };
    uint8_t *b = ring + (DATA - BASE);
    memcpy(b, "1SNd", 4), put32(b + 4, 0x100);
    put32(b + 8, 0x100 - 8 - 0x14);   /* every frame the block holds, a byte each */
    for (int i = 8 + 0x14; i < 0x100; i++) b[i] = 0x5a;
    CHECK_EQ_INT(sound_stream_block(&d, DATA), 0);
    static int16_t out[2 * 256];
    for (int i = 0; i < 8; i++) (void)sound_mix_frames(&d.mixer, out, 256);
    CHECK(1);
}

/* a compressed stereo '1SNh' block at the ring's end whose first data (+0x28) says 0x7fffffff samples: decoded to the ring's end,
 * not past it */
static void test_header_past_ring(void)
{
    enum { AT = BASE + RING - 0x60 };
    start_at(AT, 0x60, 0x7fffffff, 0, 1, 2, 22050);
    for (int i = 0x3c; i < 0x60; i++) ring[AT - BASE + i] = 0x5a;
    static int16_t out[2 * 256];
    for (int i = 0; i < 4; i++) (void)sound_mix_frames(&d.mixer, out, 256);
    CHECK(1);
}

int main(void)
{
    ring = calloc(RING + SOUND_RING_PAD, 1);
    CHECK(ring != NULL);
    if (ring == NULL) return TEST_REPORT();
    test_packed_ring_end();
    test_header_past_ring();
    test_packed_index();
    test_packed_count();
    start(0);   /* the CD's */
    CHECK_EQ_INT(ring[HEADER - BASE + 0x1c], CHANNEL);   /* the start writes the channel it played on, as the original does */
    CHECK(d.voice[CHANNEL].on);
    start(155);
    CHECK_EQ_INT(ring[HEADER - BASE + 0x1c], CHANNEL);
    CHECK(d.voice[CHANNEL].on);
    free(ring);
    return TEST_REPORT();
}
