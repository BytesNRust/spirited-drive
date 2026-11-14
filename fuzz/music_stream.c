/* A music stream (.asf / .as4, the CD's or a mod's) as the menus and the race play it: the reader puts its chunks in the ring, the
 * stream queue takes them ('1SNh' the header, '1SNd' data, '1SNl' a loop, '1SNe' the end) and the mixer plays them */
#include "fuzz/fuzz.h"
#include "game/audio/music_reader.h"
#include "game/audio/sound_driver.h"
#include "game/race/race_music.h"

#include <stdlib.h>
#include <string.h>

enum { RATE = 0x5622, FRAMES = 256, STEPS = 600, RING_BASE = 0x10000000 };

static MusicReader reader;

static void loop(void *ctx, uint32_t file, uint32_t offset) { (void)ctx, (void)file, music_reader_seek(&reader, offset); }

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    static SoundDriver d;
    memset(&d, 0, sizeof d);
    sound_driver_wire(&d);
    sound_channels_reset(&d.channels);
    d.channels.open = 1;
    sound_driver_init(&d, RATE);
    uint8_t *ring = calloc(RACE_MUSIC_RING_SMALL + SOUND_RING_PAD, 1);
    if (ring == NULL) return 0;
    const SoundRing r = { ring, RING_BASE, RACE_MUSIC_RING_SMALL };
    music_reader_open(&reader, r, data, size);
    d.stream.ring = r, d.stream.loop = loop, d.stream.loop_ctx = NULL;
    sound_stream_reset(&d.stream, 1);
    static int16_t out[2 * FRAMES];
    int started = 0;
    for (int step = 0; step < STEPS; step++) {
        const uint32_t block = music_reader_next(&reader);
        if (block != 0) (void)sound_stream_block(&d, block);
        if (!started && d.stream.state == 3) started = sound_stream_start(&d) >= 0;
        (void)sound_mix_frames(&d.mixer, out, FRAMES);
        if (d.stream.state == 0 || d.stream.state == 1) break;
    }
    sound_stream_drop(&d.stream);
    free(ring);
    return 0;
}
