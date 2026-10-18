/* The menus' sound and music over drive_audio (menu_audio.h) */
#include "game/frontend/menu_audio.h"

#include "game/frontend/config.h"
#include "game/frontend/menu_sound.h"
#include "game/race/race_sound.h"
#include "game/sys/resource.h"
#include "game/sys/timer.h"
#include "game/viewer/drive_audio.h"
#include "platform/platform.h"

#include <stdio.h>
#include <string.h>

enum { MUSIC_SETTING = 0x73, MUSIC_VOLUME = 0x77, MUSIC_TRACKS = 12 };  /* 00524e63, 00524e67 in config.dat */

static MenuSound sound;       /* sound_on, the voices */
static RaceSoundBanks banks;  /* front_mw.bnk */
static int16_t small;         /* 004c5cca */

static int16_t config_short(int at)
{
    int16_t v;
    memcpy(&v, config_current()->image + at, sizeof v);
    return v;
}

/* the music's words from config.dat as they are now (Options changes them) */
static void music_words(void) { drive_audio_music_set(config_short(MUSIC_SETTING), config_short(MUSIC_VOLUME), small); }

void menu_audio_open(void)
{
    sound.sound_on = drive_audio_device_open(resource_root());
    music_words();
}

void menu_audio_close(void)
{
    menu_audio_track_free();
    drive_audio_close();
    banks = (RaceSoundBanks){ 0 };
    sound.sound_on = 0;
}

void menu_audio_bank_load(void)
{
    if (sound.sound_on && banks.count == 0) drive_audio_load(&banks, "front_mw.bnk");
}

/* the voices stopped first: 0048edfc leaves them playing from the file freed after it, which the original's heap
 * kept readable */
void menu_audio_bank_free(void)
{
    if (banks.count == 0) return;
    drive_audio_voice_stop(sound.voice), drive_audio_voice_stop(sound.voice_4a);
    const RaceSoundBankOps *ops = drive_audio_bank_ops();
    for (int32_t i = banks.count - 1; i >= 0; i--) ops->unregister(ops->ctx, banks.registered[i]), ops->free(ops->ctx, banks.file[i]);
    banks.count = 0;
}

static int32_t voice_start(void *ctx, int32_t s, int32_t pan, int32_t volume, int32_t pitch)
{
    (void)ctx;
    return drive_audio_sound(s, pan, volume, pitch);
}

void menu_audio_sound(int32_t s)
{
    const MenuSoundOps ops = { .voice_start = voice_start };
    menu_sound_play(&sound, config_current(), s, &ops);
    drive_audio_pump();
}

void menu_audio_music_start(const char *name)
{
    music_words();
    drive_audio_music_play(name);
}

/* 004640d0 fills the stream until it holds 0x64000 bytes, which the original's mixing thread drains meanwhile; here
 * nothing drains it inside that loop and some tracks' blocks stop short of it in the ring (rock tracks after a race:
 * 363771 queued, the reader at 0), so the menus feed it a block at a time as 00464240 does */
void menu_audio_music_feed(void) { drive_audio_music_pump(), drive_audio_pump(); }
void menu_audio_music_service(void) { drive_audio_music_pump(), drive_audio_pump(); }
void menu_audio_music_fade_out(void) { drive_audio_music_fade(); }
void menu_audio_music_end(void) { drive_audio_music_stop(); }
void menu_audio_music_fade_to(int32_t volume) { drive_audio_music_volume((int16_t)volume); }

/* 004c55ac */
static const char *const NAMES[MUSIC_TRACKS] = {
    "rock1.as4", "rock2.as4", "rock3.as4", "rock4.as4", "rock5.as4", "rock6.as4",
    "techno1.as4", "techno2.as4", "techno3.as4", "techno4.as4", "techno5.as4", "techno6.as4"
};
static int32_t pick_rand(void *ctx, int32_t n) { (void)ctx; return (int32_t)(timer_ticks() % (uint32_t)n); }
static void pick_play(void *ctx, int32_t track)
{
    (void)ctx;
    if (track >= 0 && track < MUSIC_TRACKS) menu_audio_music_start(NAMES[track]);
}

/* 00443730 here the clock's ticks, not the C library's rand: the menus have no race seed to keep */
void menu_audio_music_pick(void)
{
    const RaceMusicOps ops = { NULL, pick_rand, pick_play };
    race_music_pick(&ops, config_short(MUSIC_SETTING), &small);
}

void menu_audio_music_by_idle(void)
{
    const RaceMusicOps ops = { NULL, pick_rand, pick_play };
    race_music_by_idle(&ops, config_short(MUSIC_SETTING), (int32_t)timer_ticks());
}

/* The track music's file, 00483e80's block: one at a time (00524828), handle 1 */
enum { TRACK_BLOCK = 1 };
static uint8_t *track_file;
static size_t track_size;

static void track_wait(void *ctx) { (void)ctx, menu_audio_music_feed(); }  /* 004640d0's place: the menus' feed */

static int32_t track_load(void *ctx, const char *path, int32_t flags)
{
    (void)ctx, (void)flags;
    char full[1024];
    snprintf(full, sizeof full, "%s/%s", resource_root(), path);
    size_t size = 0;
    uint8_t *file = platform_read_file(full, &size);
    if (file == NULL) return 0;
    drive_audio_speech_stop();  /* nothing left playing from the file it replaces */
    platform_free_file(track_file);
    track_file = file, track_size = size;
    return TRACK_BLOCK;
}

static void track_block_free(void *ctx, int32_t block)
{
    (void)ctx;
    if (block != TRACK_BLOCK || track_file == NULL) return;
    drive_audio_speech_stop();
    platform_free_file(track_file);
    track_file = NULL;
}

static void track_volume(void *ctx, int32_t a, int32_t b, int32_t volume) { (void)ctx, drive_audio_speech_setup(a, b, (uint32_t)volume); }
static void track_begin(void *ctx, int32_t music, int32_t percent)
{
    (void)ctx;
    if (music == TRACK_BLOCK && track_file != NULL && drive_audio_speech_add(track_file, track_size, (uint32_t)percent) != 0)
        fprintf(stderr, "menu_audio: the track music is not a sample the driver plays\n");
}
static void track_play(void *ctx) { (void)ctx, drive_audio_speech_play(); }
static void track_stop(void *ctx) { (void)ctx, drive_audio_speech_stop(); }

static const MenuSoundOps TRACK_OPS = { .wait = track_wait, .file_load = track_load, .block_free = track_block_free,
                                        .track_volume = track_volume, .track_begin = track_begin,
                                        .track_play = track_play, .track_stop = track_stop };

static const char *speech_dir(void) { return resource_german() ? "frontend/gspeech/" : "frontend/speech/"; }

void menu_audio_track_music(const char *name) { menu_sound_track_music_start(&sound, config_current(), speech_dir(), name, &TRACK_OPS); }
void menu_audio_track_play(void) { menu_sound_track_music_play(&sound, config_current(), &TRACK_OPS); }
void menu_audio_track_stop(void) { menu_sound_track_music_stop(&sound, config_current(), &TRACK_OPS); }
void menu_audio_track_free(void) { menu_sound_track_music_free(&sound, config_current(), &TRACK_OPS); }

/* A movie's sound (004423d4's sound channel, glue): the EACS sample on the speech queue's channels 4 and 4 at the
 * track music's volume (00524e69 x 3 / 2, at most 0x7f) */
int menu_audio_movie_sound(const uint8_t *eacs, size_t size)
{
    enum { CHANNEL = 4, SOUND_VOLUME = 0x79, VOLUME_MAX = 0x7f };
    if (!sound.sound_on) return 0;
    int32_t volume = config_short(SOUND_VOLUME) * 3 / 2;
    if (volume > VOLUME_MAX) volume = VOLUME_MAX;
    drive_audio_speech_setup(CHANNEL, CHANNEL, (uint32_t)volume);
    if (drive_audio_speech_add(eacs, size, 0) != 0) return 0;
    drive_audio_speech_play();
    return 1;
}

void menu_audio_movie_sound_stop(void) { drive_audio_speech_stop(); }
