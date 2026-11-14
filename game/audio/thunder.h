/* port (rain_v2): the storm's thunder. One short mono sample (data/sounds/thunder.wav, 16-bit at the mixer's rate) played over the race's own mix, after a delay and at a volume
 * the caller chooses (weather_thunder_delay, weather_thunder_volume). A few voices at once; a new one with all in use replaces the oldest. */
#ifndef TNFS_THUNDER_H
#define TNFS_THUNDER_H

#include <stddef.h>
#include <stdint.h>

enum { THUNDER_VOICES = 4, THUNDER_GAIN_PERCENT = 70 };

typedef struct {
    int16_t *pcm;                 /* the sample (owned), NULL when none */
    uint32_t frames;
    uint32_t rate;                /* the mixer's rate the sample was made for */
    struct {
        uint32_t wait;            /* frames until it starts */
        uint32_t at;              /* frames played */
        int volume;               /* percent */
        int live;
    } voice[THUNDER_VOICES];
} Thunder;

/* The sample from a WAV file's bytes: PCM, 16-bit, mono, at `rate`. 1 and the sample kept, 0 and nothing changed otherwise. */
int thunder_load(Thunder *t, const uint8_t *wav, size_t size, uint32_t rate);
void thunder_free(Thunder *t);

/* A voice that starts `delay_ticks` (30 a second) from now at `volume_percent` (0..100). Does nothing without a sample. */
void thunder_schedule(Thunder *t, int delay_ticks, int volume_percent);

/* Adds the voices' next `frames` stereo frames into `out` (interleaved), saturating. */
void thunder_mix(Thunder *t, int16_t *out, uint32_t frames);
/* The same with each frame's `gain` (0..HUSH_UNITY, hush.h) on the voices; NULL whole */
void thunder_mix_gained(Thunder *t, int16_t *out, uint32_t frames, const int32_t *gain);
/* The claps sounding stopped; the ones still to come wait on (the weather hushed: hush.h) */
void thunder_quiet(Thunder *t);

/* How many voices are waiting or playing. */
int thunder_active(const Thunder *t);

#endif
