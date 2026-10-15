#include "game/viewer/drive_audio.h"

#include "game/audio/music_reader.h"
#include "game/audio/sound_bank.h"
#include "game/audio/sound_driver.h"
#include "game/race/race_music.h"
#include "platform/platform.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

enum {
    MIX_RATE = 0x5622,      /* 0049212c's */
    MIX_CHUNK = 0x200,      /* frames per 00497cb2 call (at most 0x400) */
    QUEUE_FRAMES = 0x600,   /* ~70 ms ahead */
    SOUND_VOLUME = 0x7f,    /* 00524e69 */
    MUSIC_VOLUME = 100,     /* 00524e65 */
    RAND_SEED = 1,          /* the C library's rand() before any srand */
    TICK_MS = 10,           /* 0048c290's 100 Hz thread runs 004a4ac0 each tick */
    TICK_CATCH_UP = 10,     /* ticks run at most per pump; a longer stall is dropped */
    MUSIC_RING_BASE = 0x10000000,  /* the ring's first address (the stream's links are 32-bit) */
    MUSIC_READER = 1        /* 00524830: the one reader */
};

static SoundDriver driver;
static SoundBank banks[RACE_SOUND_BANKS];
static uint8_t *files[RACE_SOUND_BANKS];
static const char *root;
static int open_;
static uint32_t ticked_ms;

static int32_t voice_start(void *user, int32_t sound, int32_t pan, uint8_t volume, uint8_t pitch)
{
    (void)user;
    return sound_play(&driver, sound, pan, volume, pitch);
}

static void voice_volume(void *user, int32_t voice, int32_t volume) { (void)user, sound_set_volume(&driver, voice, volume); }
static void voice_pitch(void *user, int32_t voice, int32_t pitch) { (void)user, sound_set_pitch(&driver, voice, pitch); }
static void voice_pan(void *user, int32_t voice, int32_t pan) { (void)user, sound_set_pan(&driver, voice, pan); }
static void voice_stop(void *user, int32_t voice) { (void)user, sound_stop(&driver, voice); }

static int32_t voice_stopped(void *user, int32_t voice)
{
    (void)user;
    return sound_handle_stopped(&driver.channels, voice);
}

int drive_audio_open(RaceState *race, const char *data_root)
{
    root = data_root;
    sound_channels_reset(&driver.channels);  /* 0049212c: 00497b34 first */
    open_ = platform_audio_open(MIX_RATE);
    if (open_) {
        driver.rand_seed = RAND_SEED;
        sound_driver_init(&driver, MIX_RATE);
        driver.channels.open = 1;  /* 004c7b54 */
        ticked_ms = platform_ticks_ms();
    }
    RaceEntry *followed = &race->entries[race_slot(race->player)];
    followed->sound_volume = open_ ? SOUND_VOLUME : 0;
    followed->music_volume = open_ ? MUSIC_VOLUME : 0;
    followed->sound_mode = 0;
    followed->sound_on = open_;
    race->unknown_52b30e = (int16_t)open_;
    race->sounds.voice_start = voice_start;
    race->sounds.voice_volume = voice_volume;
    race->sounds.voice_pitch = voice_pitch;
    race->sounds.voice_pan = voice_pan;
    race->sounds.voice_stopped = voice_stopped;
    race->sounds.voice_stop = voice_stop;
    return open_;
}

/* The CD's names are upper case */
static uint8_t *read_bank(const char *name, size_t *size)
{
    char path[512];
    int n = snprintf(path, sizeof path, "%s/SIMDATA/SOUNDBNK/", root);
    for (int i = 0; name[i] != '\0' && n < (int)sizeof path - 1; i++) path[n++] = (char)toupper((unsigned char)name[i]);
    path[n] = '\0';
    return platform_read_file(path, size);
}

void drive_audio_load(void *user, const char *name)
{
    RaceSoundBanks *loaded = user;
    const int32_t i = loaded->count;
    if (i < 0 || i >= RACE_SOUND_BANKS) return;
    size_t size = 0;
    uint8_t *file = read_bank(name, &size);
    const int32_t slot = file != NULL ? sound_bank_register(&driver, &banks[i], file, size) : SOUND_BANK_NONE;
    if (slot < 0) {  /* the original stops the game here (004c6f88) */
        fprintf(stderr, "drive_audio: bank %s not loaded (%d)\n", name, slot);
        platform_free_file(file);
        return;
    }
    files[i] = file;
    loaded->registered[i] = (uint32_t)slot;
    loaded->file[i] = (uint32_t)i;
    loaded->count = i + 1;
}

static void bank_unregister(void *ctx, uint32_t bank) { (void)ctx, sound_bank_unregister(&driver, (int32_t)bank); }

static void bank_free(void *ctx, uint32_t file)
{
    (void)ctx;
    if (file >= RACE_SOUND_BANKS) return;
    platform_free_file(files[file]);
    files[file] = NULL;
}

const RaceSoundBankOps *drive_audio_bank_ops(void)
{
    static const RaceSoundBankOps ops = { NULL, bank_unregister, bank_free };
    return &ops;
}

void drive_audio_pump(void)
{
    if (!open_) return;
    const uint32_t now = platform_ticks_ms();
    for (int i = 0; i < TICK_CATCH_UP && now - ticked_ms >= TICK_MS; i++, ticked_ms += TICK_MS) sound_driver_tick(&driver);
    if (now - ticked_ms >= TICK_MS) ticked_ms = now;
    static int16_t out[2 * MIX_CHUNK];
    for (int i = 0; i < QUEUE_FRAMES / MIX_CHUNK && platform_audio_queued() < QUEUE_FRAMES; i++) {
        sound_mix_frames(&driver.mixer, out, MIX_CHUNK);
        platform_audio_push(out, MIX_CHUNK);
    }
}

/* The music: the race's words, the reader, its ring and the file */
static RaceMusic music;
static MusicReader reader;
static uint8_t *music_ring;
static uint8_t *music_file;
static char music_path[512];
static uint32_t music_ticks_ms;

static void music_service(void *ctx) { (void)ctx, drive_audio_pump(); }  /* 00475c14(0): the sound kept going */

/* 00473334: the 100 Hz ticks since its last call */
static int32_t music_ticks(void *ctx)
{
    (void)ctx;
    const uint32_t now = platform_ticks_ms(), ticks = (now - music_ticks_ms) / TICK_MS;
    music_ticks_ms += ticks * TICK_MS;
    return (int32_t)ticks;
}

static uint32_t music_next(void *ctx, uint32_t id) { (void)ctx, (void)id; return music_ring != NULL ? music_reader_next(&reader) : 0; }
static int32_t music_queued(void *ctx) { (void)ctx; return sound_stream_queued(&driver.stream, SOUND_STREAM_BYTES); }
static void music_block(void *ctx, uint32_t block) { (void)ctx, sound_stream_block(&driver, block); }
static int32_t music_start(void *ctx) { (void)ctx; return sound_stream_start(&driver); }
static void music_set_volume(void *ctx, int32_t handle, int32_t volume) { (void)ctx, sound_set_volume(&driver, handle, volume); }
static void music_ramp(void *ctx, int32_t handle, int32_t steps, int32_t volume) { (void)ctx, sound_volume_ramp(&driver, handle, steps, volume); }
static void music_stop(void *ctx, int32_t handle) { (void)ctx, sound_stop(&driver, handle); }
static int32_t music_stopped(void *ctx, int32_t handle) { (void)ctx; return sound_handle_stopped(&driver.channels, handle); }

static void music_close(void *ctx, uint32_t id)
{
    (void)ctx, (void)id;
    platform_free_file(music_file);
    free(music_ring);
    music_file = NULL, music_ring = NULL;
    reader = (MusicReader){ 0 };
}

static void music_settle(void *ctx) { (void)ctx; }

/* 00473d43: 005242d8's folder and the name; the CD's names are upper case */
static void music_name(void *ctx, const char *name)
{
    (void)ctx;
    int n = snprintf(music_path, sizeof music_path, "%s/FRONTEND/MUSIC/", root);  /* resource.h's PATH_MUSIC */
    for (const char *c = name; *c != '\0' && n < (int)sizeof music_path - 1; c++) music_path[n++] = (char)toupper((unsigned char)*c);
    music_path[n] = '\0';
}

/* 0048a674: the ring */
static uint32_t music_open(void *ctx, int32_t ring)
{
    (void)ctx;
    music_close(NULL, MUSIC_READER);
    music_ring = calloc((size_t)ring, 1);
    reader.ring = (SoundRing){ music_ring, MUSIC_RING_BASE, music_ring != NULL ? (uint32_t)ring : 0 };
    return MUSIC_READER;
}

/* 0048aa78: the file read whole, the reader over it from its start */
static void music_request(void *ctx, uint32_t id)
{
    (void)ctx, (void)id;
    size_t size = 0;
    music_file = music_ring != NULL ? platform_read_file(music_path, &size) : NULL;
    if (music_file == NULL) {
        fprintf(stderr, "drive_audio: music %s not loaded\n", music_path);
        music.sound_on = 0;
        return;
    }
    music_reader_open(&reader, reader.ring, music_file, size);
}

/* 0048ab4c: the loop's seek */
static void music_loop(void *ctx, uint32_t file, uint32_t offset) { (void)ctx, (void)file, music_reader_seek(&reader, offset); }

static void music_reset(void *ctx, uint32_t id)
{
    (void)ctx;
    driver.stream.ring = reader.ring;
    driver.stream.loop = music_loop;
    driver.stream.loop_ctx = NULL;
    sound_stream_reset(&driver.stream, id);
}

static const RaceMusicStreamOps music_ops = { NULL, music_service, music_ticks, music_next, music_queued, music_block, music_start,
                                              music_set_volume, music_ramp, music_stop, music_stopped, music_close, music_settle,
                                              music_name, music_open, music_request, music_reset };

void drive_audio_music_set(int16_t setting, int16_t volume, int16_t small)
{
    music.setting = setting, music.volume = volume, music.small = small, music.sound_on = (int16_t)open_;
    music_ticks_ms = platform_ticks_ms();
}

void drive_audio_music_play(const char *name) { race_music_play(&music_ops, &music, name); }
void drive_audio_music_fill(void) { race_music_fill(&music_ops, &music); }
void drive_audio_music_pump(void) { race_music_pump(&music_ops, &music); }
void drive_audio_music_volume(int16_t volume) { race_music_volume(&music_ops, &music, volume); }

void drive_audio_close(void)
{
    if (music.open) music_stop(NULL, music.handle), race_music_stop(&music_ops, &music);
    music_close(NULL, MUSIC_READER);
    sound_bank_unregister(&driver, SOUND_BANK_ALL);
    for (int i = 0; i < RACE_SOUND_BANKS; i++) bank_free(NULL, (uint32_t)i);
    if (open_) platform_audio_close();
    open_ = 0;
}
