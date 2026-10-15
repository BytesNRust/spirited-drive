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

/* The race's music (game/race/race_music.c) on the driver's stream and the
 * port's reader (music_reader.h) over frontend/music/: 00464428 the track
 * `name` (004c55ac's), 004640d0 the stream filled ahead, 00464240 a block a
 * frame (its voice started once enough is queued), 00464604 the volume
 * ramped to (0046463c at once). `setting` is 00524e63 (0 no music), `volume` 00524e65 and
 * `small` 004c5cca (the small ring, which race_run sets); the device
 * closed: no music. A track whose
 * file does not load leaves the music off (the original's reader thread
 * fails without a word). */
void drive_audio_music_set(int16_t setting, int16_t volume, int16_t small);
void drive_audio_music_play(const char *name);
void drive_audio_music_fill(void);
void drive_audio_music_pump(void);
void drive_audio_music_volume(int16_t volume);
void drive_audio_music_volume_now(int16_t volume);  /* 0046463c */
int16_t drive_audio_music_setting(void);            /* 00524e63 */

void drive_audio_close(void);

#endif
