/* port (rain_tyre_roll): a rain or roll loop's voice. One looping mono sample (data/sounds/<file>.wav, 16-bit at the mixer's rate) mixed over the race's own mix, its level, pitch and
 * pan set by the caller (rain_roll.h, rain_cars.h) and eased so a change is not a click, and silent when nobody sets it for half a second. A voice may play another one's sample
 * (wet_roll_share: the cars' sprays share one). */
#ifndef TNFS_WET_ROLL_H
#define TNFS_WET_ROLL_H

#include "game/audio/thunder.h"

#include <stdint.h>

enum {
    WET_ROLL_MAX_PERCENT = 600,   /* a level above 100 lifts a quiet sample; the mix saturates */
    WET_ROLL_PAN_LEFT = 0,        /* the driver's pans (sound_pan_gains): 0 the left alone, 0x40 both at full, 0x7f the right alone */
    WET_ROLL_PAN_CENTRE = 0x40,
    WET_ROLL_PAN_RIGHT = 0x7f
};

typedef struct {
    Thunder sample;          /* the sample (thunder.h's loader: PCM, 16-bit, mono, at the rate) */
    const Thunder *shared;   /* another voice's sample it plays instead, NULL for its own */
    uint64_t position;       /* where it plays, in 1/65536 of a frame */
    int target;              /* the level asked, percent * 256 */
    int level;               /* the level now, percent * 256, eased toward the target */
    int pitch;               /* permille of the sample's rate */
    int pan;                 /* the pan asked, WET_ROLL_PAN_LEFT .. _RIGHT */
    int pan_now;             /* the pan now, * 256, eased toward it */
    uint32_t idle;           /* frames since the caller last set it: a caller that stopped (a pause, the menus, the race's end) leaves it to fall silent after half a second */
} WetRoll;

/* The sample from a WAV file's bytes: 1 and kept, 0 and nothing changed otherwise. */
int wet_roll_load(WetRoll *w, const uint8_t *wav, size_t size, uint32_t rate);
/* A silent voice that plays `t` (kept by its owner while the voice plays). */
void wet_roll_share(WetRoll *w, const Thunder *t);
void wet_roll_free(WetRoll *w);

/* The level (percent 0..WET_ROLL_MAX_PERCENT) and the pitch (permille of the sample's own rate, 1000 as it is) it eases to. */
void wet_roll_set(WetRoll *w, int percent, int pitch_permille);
/* The pan it eases to (a tenth of a second across), WET_ROLL_PAN_LEFT .. _RIGHT; WET_ROLL_PAN_CENTRE unless set. */
void wet_roll_pan(WetRoll *w, int pan);
/* Silent at once, its level and the level asked dropped (the rain's bus hushed: rain_bus.h); a later wet_roll_set eases it back from silence. */
void wet_roll_quiet(WetRoll *w);
/* Play on from `permille` of the loop (a voice taken by another car starts elsewhere, so the cars do not sound in phase). */
void wet_roll_seek(WetRoll *w, int permille);

/* The driver's gain of `channel` (0 left, 1 right) for `pan`, 0..0x7f. */
int wet_roll_pan_gain(int pan, int channel);

/* Adds the next `frames` stereo frames into `out` (interleaved), saturating. Nothing while silent. */
void wet_roll_mix(WetRoll *w, int16_t *out, uint32_t frames);
/* The same with each frame's `gain` (0..HUSH_UNITY, hush.h) on the voice; NULL whole */
void wet_roll_mix_gained(WetRoll *w, int16_t *out, uint32_t frames, const int32_t *gain);
/* The same into a 32-bit bus, not saturated (the caller sums the bus with the race's mix: drive_audio.c) */
void wet_roll_mix32(WetRoll *w, int32_t *out, uint32_t frames);

#endif
