/* --drive's sound: the EA driver (game/audio) mixed into the platform's
 * 16-bit stereo stream at 22050 Hz, the race's voice hooks on it and the
 * race's banks loaded as 004438c8 loads them. */
#ifndef TNFS_DRIVE_AUDIO_H
#define TNFS_DRIVE_AUDIO_H

#include "game/race/race_sound.h"
#include "game/race/race_state.h"

#include <stddef.h>
#include <stdint.h>

/* 0049212c / 00497bb5 and 004275f8's sound words: with a device, the driver
 * open at 0x5622 Hz, the followed slot's entry given the frontend's
 * defaults (00429278: sound volume 0x7f, music 100, mode 0) and +0x29 and
 * 0052b30e set; without one, all 0 (the race plays no sound). The voice
 * hooks go on race->sounds either way. */
int drive_audio_open(RaceState *race, const char *data_root);

/* The device part of drive_audio_open alone (the menus' sound): 1 with a device */
int drive_audio_device_open(const char *data_root);

/* 0048eea0 on the driver: `sound` of the banks loaded at pan, volume and pitch; its voice (0 without a device) */
int32_t drive_audio_sound(int32_t sound, int32_t pan, int32_t volume, int32_t pitch);
/* port (rain_v2): a thunder clap `delay_ticks` (30 a second) from now at `volume_percent`, over the race's own mix; nothing without a device or without data/sounds/thunder.wav */
void drive_audio_thunder(int delay_ticks, int volume_percent);
/* port (rain_tyre_roll): the rain's layers (rain_roll.h), each at its percent (0 silent) and eased: `out_percent` the rain round a stopped car (data/sounds/rain_stopped_out.wav), `in_percent` the rain
 * on its roof (rain_stopped_in.wav), `bed_percent` the rain round a moving car (rain_moving_out.wav), `move_in_percent` the rain on a moving car with its tyres' hiss, heard in the cockpit
 * (rain_moving_in.wav); nothing without a device or without the file */
void drive_audio_rain_roll(int out_percent, int in_percent, int bed_percent, int move_in_percent);
/* port (rain_tyre_roll): the tyre spray (rain_spray.wav) of voice `voice` (0..RAIN_CARS_VOICES - 1, rain_cars.h) at `percent`, `pitch_permille` of its rate and `pan` (WET_ROLL_PAN_*), eased;
 * `start_permille` 0..999 plays on from there in the loop (a voice taken by another car), -1 where it is */
void drive_audio_rain_spray(int voice, int percent, int pitch_permille, int pan, int start_permille);
/* port (rain_tyre_roll): the puddle splashes there are (data/sounds/rain_puddle_1.wav on, read once), 0 without a device; one played once at `percent`, `pitch_permille` and `pan` */
int drive_audio_rain_puddle_clips(void);
void drive_audio_rain_puddle(int clip, int percent, int pitch_permille, int pan);
/* port (rain_tyre_roll): the pause or the in-race menu at the race's `tick`: the weather's sounds (the thunder, the rain, the roll on snow) fall silent in a few ms (hush.h); awake
 * again once a frame sees the tick move (drive_audio_weather_wake, called by each weather hook before its levels) */
void drive_audio_weather_hush(int32_t tick);
void drive_audio_weather_wake(int32_t tick);
/* port (snow): the roll on snow (snow_roll.h) at `volume_percent` (0 silent) and `pitch_permille` of its own rate, eased; nothing without a device or without data/sounds/snow_roll.wav */
void drive_audio_snow_roll(int volume_percent, int pitch_permille);

/* 00473c68 on the driver: the voice stopped */
void drive_audio_voice_stop(int32_t voice);
/* 00473b4c on the driver: the voice's volume (0..0x7f); 00473d20: 1 once the voice has stopped (1 without a device) */
void drive_audio_voice_volume(int32_t voice, int32_t volume);
int32_t drive_audio_voice_stopped(int32_t voice);

/* 004438c8 for race_sound_load_banks: the bank from simdata/soundbnk/,
 * registered (0048ed30), noted in `banks` (user). */
void drive_audio_load(void *user, const char *name);

/* race_sound_end's ops: 0048edfc and the file freed */
const RaceSoundBankOps *drive_audio_bank_ops(void);

/* Nothing now: the device's thread mixes the driver (00497cb2) and runs its 100 Hz service (004a4ac0: the speech
 * queue, volume ramps, countdowns) on the sample clock, as 0048c290's thread did, so a load on the game's thread
 * no longer starves the sound. Kept for the places the original yielded to that thread (00475c14). Every call
 * here into the driver holds platform_audio_lock. */
void drive_audio_pump(void);

/* port (dev, env only): TNFS_AUDIO_DUMP in the headless --drive: the driver with no device, every tick's mix written to a WAV (drive_audio.c). 1 when the file opened. */
int drive_audio_offline_open(RaceState *race, const char *data_root, const char *wav_path);
void drive_audio_offline_tick(void);
void drive_audio_offline_close(void);

/* EA's speech queue (game/audio/sound_queue.c, 004c7b60) on the driver: 00492438 its two channels and volume,
 * 004924f8 an EACS file queued (refused, -1, when it is shorter than its header or than the frames it names;
 * the file stays the caller's while it plays), 004925a8 played, 00492620 stopped. The pump runs 00492330 before
 * the channel loop while it is set up, as 004a4ac0 does. */
int32_t drive_audio_speech_setup(int32_t channel_a, int32_t channel_b, uint32_t volume);
int32_t drive_audio_speech_add(const uint8_t *file, size_t size, uint32_t percent);
void drive_audio_speech_play(void);
void drive_audio_speech_stop(void);

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
void drive_audio_music_small(int16_t small);        /* 004c5cca */
/* 00464534: the track's voice stopped (0052b31e clear, the setting, the
 * sound and a track open); 004644f0: on the same terms faded out over 100
 * ticks to -1, which stops it; 00464568: the track closed once it has stopped */
void drive_audio_music_cut(void);
void drive_audio_music_fade(void);
void drive_audio_music_stop(void);

void drive_audio_close(void);

#endif
