#include "game/viewer/drive_audio.h"

#include "game/audio/sound_bank.h"
#include "game/audio/sound_driver.h"
#include "platform/platform.h"

#include <ctype.h>
#include <stdio.h>

enum {
    MIX_RATE = 0x5622,      /* 0049212c's */
    MIX_CHUNK = 0x200,      /* frames per 00497cb2 call (at most 0x400) */
    QUEUE_FRAMES = 0x600,   /* ~70 ms ahead */
    SOUND_VOLUME = 0x7f,    /* 00524e69 */
    MUSIC_VOLUME = 100,     /* 00524e65 */
    RAND_SEED = 1           /* the C library's rand() before any srand */
};

static SoundDriver driver;
static SoundBank banks[RACE_SOUND_BANKS];
static uint8_t *files[RACE_SOUND_BANKS];
static const char *root;
static int open_;

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
    static int16_t out[2 * MIX_CHUNK];
    for (int i = 0; i < QUEUE_FRAMES / MIX_CHUNK && platform_audio_queued() < QUEUE_FRAMES; i++) {
        sound_mix_frames(&driver.mixer, out, MIX_CHUNK);
        platform_audio_push(out, MIX_CHUNK);
    }
}

void drive_audio_close(void)
{
    sound_bank_unregister(&driver, SOUND_BANK_ALL);
    for (int i = 0; i < RACE_SOUND_BANKS; i++) bank_free(NULL, (uint32_t)i);
    if (open_) platform_audio_close();
    open_ = 0;
}
