/* port (rain_tyre_roll): the wet-tyre roll's voice. One looping mono sample (data/sounds/rain_tyre_roll.wav, 16-bit at the mixer's rate) mixed over the race's own mix, its level and pitch set by the
 * caller (rain_roll.h) and eased so a change is not a click, and silent when nobody sets it for half a second. */
#ifndef TNFS_WET_ROLL_H
#define TNFS_WET_ROLL_H

#include "game/audio/thunder.h"

#include <stdint.h>

enum { WET_ROLL_MAX_PERCENT = 400 };   /* a level above 100 lifts a quiet sample; the mix saturates */

typedef struct {
    Thunder sample;      /* the sample (thunder.h's loader: PCM, 16-bit, mono, at the rate) */
    uint64_t position;   /* where it plays, in 1/65536 of a frame */
    int target;          /* the level asked, percent * 256 */
    int level;           /* the level now, percent * 256, eased toward the target */
    int pitch;           /* permille of the sample's rate */
    uint32_t idle;       /* frames since the caller last set it: a caller that stopped (a pause, the menus, the race's end) leaves it to fall silent after half a second */
} WetRoll;

/* The sample from a WAV file's bytes: 1 and kept, 0 and nothing changed otherwise. */
int wet_roll_load(WetRoll *w, const uint8_t *wav, size_t size, uint32_t rate);
void wet_roll_free(WetRoll *w);

/* The level (percent 0..WET_ROLL_MAX_PERCENT) and the pitch (permille of the sample's own rate, 1000 as it is) it eases to. */
void wet_roll_set(WetRoll *w, int percent, int pitch_permille);

/* Adds the next `frames` stereo frames into `out` (interleaved), saturating. Nothing while silent. */
void wet_roll_mix(WetRoll *w, int16_t *out, uint32_t frames);

#endif
