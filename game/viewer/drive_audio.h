/* --drive's sound: the EA driver (game/audio) mixed into the platform's
 * 16-bit stereo stream at 22050 Hz, the race's voice hooks on it and the
 * race's banks loaded as 004438c8 loads them. */
#ifndef TNFS_DRIVE_AUDIO_H
#define TNFS_DRIVE_AUDIO_H

#include "game/race/race_sound.h"
#include "game/race/race_state.h"

/* 0049212c / 00497bb5 and 004275f8's sound words: with a device, the driver
 * open at 0x5622 Hz, the followed slot's entry given the frontend's
 * defaults (00429278: sound volume 0x7f, music 100, mode 0) and +0x29 and
 * 0052b30e set; without one, all 0 (the race plays no sound). The voice
 * hooks go on race->sounds either way. */
int drive_audio_open(RaceState *race, const char *data_root);

/* 004438c8 for race_sound_load_banks: the bank from simdata/soundbnk/,
 * registered (0048ed30), noted in `banks` (user). */
void drive_audio_load(void *user, const char *name);

/* race_sound_end's ops: 0048edfc and the file freed */
const RaceSoundBankOps *drive_audio_bank_ops(void);

/* Runs the driver's 100 Hz service (004a4ac0: volume ramps, countdowns)
 * for the ticks due, then keeps the stream fed: mixes (00497cb2) while
 * fewer frames are queued than a few frames' worth. */
void drive_audio_pump(void);

void drive_audio_close(void);

#endif
