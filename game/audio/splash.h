/* port (rain_tyre_roll): the puddle splashes (rain_cars.h). A few short mono samples (data/sounds/rain_puddle_<n>.wav, 16-bit at the mixer's rate), each splash played once over the
 * race's own mix at its level, rate and pan; a few at once, a new one with all in use replacing the one nearest its end. */
#ifndef TNFS_SPLASH_H
#define TNFS_SPLASH_H

#include "game/audio/thunder.h"

#include <stdint.h>

enum { SPLASH_CLIPS = 8, SPLASH_VOICES = 6, SPLASH_MAX_PERCENT = 400 };

typedef struct {
    Thunder clip[SPLASH_CLIPS];   /* the samples (thunder.h's loader) */
    int clips;                    /* loaded, the first ones */
    struct {
        int clip;
        uint64_t position;        /* 1/65536 of a frame */
        uint32_t advance;         /* per frame, 1/65536 */
        int32_t gain[2];          /* left, right: percent * 0x7f (the driver's pan gains) */
        int live;
    } voice[SPLASH_VOICES];
} Splash;

/* The next clip from a WAV file's bytes: 1 and kept (clips one more), 0 and nothing changed otherwise (or all SPLASH_CLIPS are taken). */
int splash_load(Splash *s, const uint8_t *wav, size_t size, uint32_t rate);
void splash_free(Splash *s);

/* A splash of `clip` at `percent` (0..SPLASH_MAX_PERCENT), `pitch_permille` of its rate and `pan` (WET_ROLL_PAN_*). Nothing for a clip not loaded or a level of 0. */
void splash_play(Splash *s, int clip, int percent, int pitch_permille, int pan);

/* Every splash stopped (the rain's bus hushed: rain_bus.h) */
void splash_quiet(Splash *s);

/* Adds the splashes' next `frames` stereo frames into `out` (interleaved), saturating. */
void splash_mix(Splash *s, int16_t *out, uint32_t frames);
/* The same into a 32-bit bus, not saturated */
void splash_mix32(Splash *s, int32_t *out, uint32_t frames);

/* How many are playing. */
int splash_active(const Splash *s);

#endif
