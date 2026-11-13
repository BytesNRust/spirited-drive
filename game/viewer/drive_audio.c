#include "game/sys/port_log.h"
#include "game/viewer/drive_audio.h"

#include "game/audio/music_reader.h"
#include "game/audio/sound_bank.h"
#include "game/audio/sound_driver.h"
#include "game/audio/sound_queue.h"
#include "game/audio/thunder.h"
#include "game/audio/wet_roll.h"
#include "game/sys/data_path.h"
#include "game/race/race_music.h"
#include "platform/platform.h"

#include <ctype.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

enum {
    MIX_RATE = 0x5622,      /* 0049212c's */
    TICKS_PER_SECOND = 100, /* 0048c290's thread runs 004a4ac0 at 100 Hz */
    STEPS_PER_SECOND = 30,  /* the race's steps (drive_clock.h) */
    TICK_FRAMES = MIX_RATE / TICKS_PER_SECOND,  /* at most this many frames mixed between two ticks (00497cb2 takes 0x400) */
    SOUND_VOLUME = 0x7f,    /* 00524e69 */
    MUSIC_VOLUME = 100,     /* 00524e65 */
    RAND_SEED = 1,          /* the C library's rand() before any srand */
    TICK_MS = 1000 / TICKS_PER_SECOND,
    MUSIC_RING_BASE = 0x10000000,  /* the ring's first address (the stream's links are 32-bit) */
    MUSIC_READER = 1        /* 00524830: the one reader */
};

static SoundDriver driver;
static SoundBank banks[RACE_SOUND_BANKS];
static uint8_t *files[RACE_SOUND_BANKS];
static size_t file_sizes[RACE_SOUND_BANKS];
static const char *root;
static int open_;
static int mixing;           /* the driver set up: the device's thread mixes it */
static uint32_t tick_frames;  /* the frames mixed since the last tick, x 100 */
static Thunder thunder;                                    /* port (rain_v2): the storm's thunder, over the mix */
static int thunder_tried;
static WetRoll wet_roll;                                   /* port (rain_tyre_roll): the wet road under the tyres, over the mix */
static int wet_roll_tried;
static WetRoll rain_out, rain_in;                          /* port (rain_tyre_roll): the rain round a stopped car, outside and on the roof */
static int rain_out_tried, rain_in_tried;
static WetRoll snow_roll;                                  /* port (snow): the roll on snow, the same voice with another sample */
static int snow_roll_tried;
static SoundQueue speech;                                  /* 004c7b60 */
static SoundSample speech_sample[SOUND_QUEUE_ENTRIES];     /* each queued file's header, by the writer it took */

/* The game's hold on the driver (platform_audio_lock, recursive), counted so a wait can let the device's thread
 * run (music_service) */
static int held;
static void lock(void) { platform_audio_lock(), held++; }
static void unlock(void) { held--, platform_audio_unlock(); }

/* The device's thread, the stream locked (platform_audio_lock keeps the game's changes out meanwhile): the driver
 * mixed (00497cb2) with its 100 Hz service (004a4ac0: the speech queue, volume ramps, countdowns) run every
 * TICK_FRAMES of the sample clock, as 0048c290's thread ran it beside the mixing */
/* port (dev only): TNFS_AUDIO_DUMP=file.wav writes what the mixer produced (16-bit stereo at MIX_RATE), the header kept current as it goes, so a run's
 * sound can be measured without a speaker. */
static FILE *dump_file;
static uint32_t dump_bytes;
static int dump_tried;

static void dump_frames(const int16_t *frames, uint32_t count)
{
    if (!dump_tried) {
        dump_tried = 1;
        const char *path = getenv("TNFS_AUDIO_DUMP");
        if (path != NULL && path[0] != '\0') dump_file = fopen(path, "wb");
    }
    if (dump_file == NULL) return;
    const uint32_t rate = MIX_RATE, bytes = (dump_bytes += count * 4), riff = 36 + bytes, byte_rate = rate * 4;
    uint8_t h[44] = { 'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 16, 0, 'd', 'a', 't', 'a' };
    for (int i = 0; i < 4; i++) {
        h[4 + i] = (uint8_t)(riff >> (8 * i)), h[24 + i] = (uint8_t)(rate >> (8 * i)), h[28 + i] = (uint8_t)(byte_rate >> (8 * i));
    }
    uint8_t size[4] = { (uint8_t)bytes, (uint8_t)(bytes >> 8), (uint8_t)(bytes >> 16), (uint8_t)(bytes >> 24) };
    memcpy(h + 40, size, 4);
    fseek(dump_file, 0, SEEK_SET);
    fwrite(h, 1, sizeof h, dump_file);
    fseek(dump_file, 0, SEEK_END);
    fwrite(frames, 4, count, dump_file);
    fflush(dump_file);
}

/* port (dev only, with TNFS_AUDIO_DUMP): each voice mixed alone from a copy of its record and the mean square kept per slot; every second of the sample
 * clock one line per voice that played: its slot, sample address' low bits, gains (the pan and volume as the mixer has them) and its own RMS. */
static double voice_sq[SOUND_VOICES];
static uint32_t voice_frames, voice_seen[SOUND_VOICES];

static void voice_stats(uint32_t count)
{
    if (dump_file == NULL) return;
    for (uint32_t slot = 0; slot < SOUND_VOICES; slot++) {
        const MixVoice *v = driver.mixer.voice[slot];
        if (v == NULL || v->on != 1 || (v->flags & 2) != 0) continue;   /* the plain PCM voices (the packed ones decode into a shared buffer) */
        SoundMixer solo;
        MixVoice copy = *v;
        memset(&solo, 0, sizeof solo);
        solo.voice[slot] = &copy;
        int16_t out[2 * SOUND_MIX_FRAMES];
        if (!sound_mix_frames(&solo, out, (int32_t)count)) continue;
        for (uint32_t i = 0; i < 2 * count; i++) voice_sq[slot] += (double)out[i] * out[i];
        voice_seen[slot] += count;
    }
    voice_frames += count;
    if (voice_frames < MIX_RATE) return;
    for (uint32_t slot = 0; slot < SOUND_VOICES; slot++) {
        if (voice_seen[slot] > 0) {
            const MixVoice *v = driver.mixer.voice[slot];
            port_log("audio: voice %2u rms %6.0f gains %u,%u rate %u\n", slot, sqrt(voice_sq[slot] / (2.0 * voice_seen[slot])), v != NULL ? v->gains.left >> 10 : 0,
                     v != NULL ? v->gains.right >> 10 : 0, v != NULL ? v->rate : 0);
        }
        voice_sq[slot] = 0, voice_seen[slot] = 0;
    }
    port_log("audio: -- second\n");
    voice_frames = 0;
}

static void mix(void *user, int16_t *frames, uint32_t count)
{
    (void)user;
    if (!mixing) {
        memset(frames, 0, (size_t)count * 2 * sizeof *frames);
        return;
    }
    int16_t *const all = frames;
    const uint32_t total = count;
    while (count > 0) {
        const uint32_t n = count < TICK_FRAMES ? count : TICK_FRAMES;
        voice_stats(n);
        sound_mix_frames(&driver.mixer, frames, (int32_t)n);
        thunder_mix(&thunder, frames, n);
        wet_roll_mix(&wet_roll, frames, n);
        wet_roll_mix(&rain_out, frames, n);
        wet_roll_mix(&rain_in, frames, n);
        wet_roll_mix(&snow_roll, frames, n);
        frames += 2 * n, count -= n;
        for (tick_frames += n * TICKS_PER_SECOND; tick_frames >= MIX_RATE; tick_frames -= MIX_RATE) {
            if (speech.ready) sound_queue_tick(&driver, &speech);  /* 004a4ac0: 00492330 first, while 004c7b60 */
            sound_driver_tick(&driver);
        }
    }
    dump_frames(all, total);
}

static int32_t voice_start(void *user, int32_t sound, int32_t pan, uint8_t volume, uint8_t pitch)
{
    (void)user;
    lock();
    const int32_t voice = sound_play(&driver, sound, pan, volume, pitch);
    unlock();
    return voice;
}

static void voice_volume(void *user, int32_t voice, int32_t volume)
{
    (void)user;
    lock(), sound_set_volume(&driver, voice, volume), unlock();
}

static void voice_pitch(void *user, int32_t voice, int32_t pitch)
{
    (void)user;
    lock(), sound_set_pitch(&driver, voice, pitch), unlock();
}

static void voice_pan(void *user, int32_t voice, int32_t pan)
{
    (void)user;
    lock(), sound_set_pan(&driver, voice, pan), unlock();
}

static void voice_stop(void *user, int32_t voice)
{
    (void)user;
    lock(), sound_stop(&driver, voice), unlock();
}

static int32_t voice_stopped(void *user, int32_t voice)
{
    (void)user;
    lock();
    const int32_t stopped = sound_handle_stopped(&driver.channels, voice);
    unlock();
    return stopped;
}

/* port (rain_v2): the thunder sample, read once from data/sounds/thunder.wav (a missing or unusable file leaves the storm silent, logged once). The caller holds the lock. */
static void thunder_read(void)
{
    if (thunder_tried) return;
    thunder_tried = 1;
    char found[DATA_PATH_CANDIDATES][DATA_PATH_MAX];
    (void)data_path_candidates("sounds/thunder.wav", found);
    for (int i = 0; i < DATA_PATH_CANDIDATES; i++) {
        if (found[i][0] == '\0') continue;
        size_t size = 0;
        uint8_t *file = platform_read_file(found[i], &size);
        if (file == NULL) continue;
        const int ok = thunder_load(&thunder, file, size, MIX_RATE);
        platform_free_file(file);
        port_log("thunder: %s: %s\n", found[i], ok ? "loaded" : "not a 16-bit mono WAV at the mixer's rate");
        if (ok) return;
    }
    port_log("thunder: no sounds/thunder.wav: the storm is silent\n");
}

/* port (rain_tyre_roll, snow): a roll voice's sample, read once from data/sounds/<file> (a missing or unusable file leaves it silent, logged once). The caller holds the lock. */
static void roll_read(WetRoll *voice, int *tried, const char *file)
{
    if (*tried) return;
    *tried = 1;
    char rel[64];
    snprintf(rel, sizeof rel, "sounds/%s", file);
    char found[DATA_PATH_CANDIDATES][DATA_PATH_MAX];
    (void)data_path_candidates(rel, found);
    for (int i = 0; i < DATA_PATH_CANDIDATES; i++) {
        if (found[i][0] == '\0') continue;
        size_t size = 0;
        uint8_t *bytes = platform_read_file(found[i], &size);
        if (bytes == NULL) continue;
        const int ok = wet_roll_load(voice, bytes, size, MIX_RATE);
        platform_free_file(bytes);
        port_log("wet_roll: %s: %s\n", found[i], ok ? "loaded" : "not a 16-bit mono WAV at the mixer's rate");
        if (ok) return;
    }
    port_log("wet_roll: no %s: the roll is silent\n", rel);
}

void drive_audio_rain_roll(int out_percent, int in_percent, int moving_percent, int pitch_permille)
{
    if (!open_) return;
    lock();
    if (out_percent > 0) roll_read(&rain_out, &rain_out_tried, "rain_stopped_out.wav");
    if (in_percent > 0) roll_read(&rain_in, &rain_in_tried, "rain_stopped_in.wav");
    if (moving_percent > 0) roll_read(&wet_roll, &wet_roll_tried, "rain_tyre_roll.wav");
    wet_roll_set(&rain_out, out_percent, 1000);
    wet_roll_set(&rain_in, in_percent, 1000);
    wet_roll_set(&wet_roll, moving_percent, pitch_permille);
    unlock();
}

void drive_audio_snow_roll(int volume_percent, int pitch_permille)
{
    if (!open_) return;
    lock();
    if (volume_percent > 0) roll_read(&snow_roll, &snow_roll_tried, "snow_roll.wav");
    wet_roll_set(&snow_roll, volume_percent, pitch_permille);
    unlock();
}

void drive_audio_thunder(int delay_ticks, int volume_percent)
{
    if (!open_) return;
    lock();
    thunder_read();
    port_log("thunder: clap in %d ticks at %d%%\n", delay_ticks, volume_percent);
    thunder_schedule(&thunder, delay_ticks, volume_percent);
    unlock();
}

int drive_audio_device_open(const char *data_root)
{
    root = data_root;
    open_ = platform_audio_open(MIX_RATE, mix, NULL);
    lock();
    sound_channels_reset(&driver.channels);  /* 0049212c: 00497b34 first */
    if (open_) {
        driver.rand_seed = RAND_SEED;
        sound_driver_init(&driver, MIX_RATE);
        driver.channels.open = 1;  /* 004c7b54 */
        tick_frames = 0;
        mixing = 1;
    }
    unlock();
    return open_;
}

int32_t drive_audio_sound(int32_t sound, int32_t pan, int32_t volume, int32_t pitch)
{
    if (!open_) return 0;
    lock();
    const int32_t voice = sound_play(&driver, sound, pan, volume, pitch);
    unlock();
    return voice;
}

void drive_audio_voice_stop(int32_t voice)
{
    if (open_) voice_stop(NULL, voice);
}

void drive_audio_voice_volume(int32_t voice, int32_t volume)
{
    if (open_) voice_volume(NULL, voice, volume);
}

int32_t drive_audio_voice_stopped(int32_t voice)
{
    return open_ ? voice_stopped(NULL, voice) : 1;
}

int drive_audio_open(RaceState *race, const char *data_root)
{
    drive_audio_device_open(data_root);
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
    lock();
    const int32_t slot = file != NULL ? sound_bank_register(&driver, &banks[i], file, size) : SOUND_BANK_NONE;
    unlock();
    if (slot < 0) {  /* the original stops the game here (004c6f88) */
        port_log("drive_audio: bank %s not loaded (%d)\n", name, slot);
        platform_free_file(file);
        return;
    }
    files[i] = file, file_sizes[i] = size;
    loaded->registered[i] = (uint32_t)slot;
    loaded->file[i] = (uint32_t)i;
    loaded->count = i + 1;
}

static void bank_unregister(void *ctx, uint32_t bank)
{
    (void)ctx;
    lock(), sound_bank_unregister(&driver, (int32_t)bank), unlock();
}

/* under the lock: a voice still on the file stops mixing it before it goes. 0048edfc leaves the voices playing (the
 * original's freed block kept its bytes); here the device's thread would read freed memory (a replay's teardown,
 * race_sound_end, with a crash sound still running) */
static void bank_free(void *ctx, uint32_t file)
{
    (void)ctx;
    if (file >= RACE_SOUND_BANKS) return;
    lock();
    const uint8_t *from = files[file], *to = from != NULL ? from + file_sizes[file] : NULL;
    for (uint32_t k = 0; from != NULL && k < SOUND_VOICES; k++) {
        const MixVoice *v = driver.mixer.voice[k];
        if (v != NULL && v->on && v->data >= from && v->data < to) sound_voice_stop(&driver.mixer, k);
    }
    platform_free_file(files[file]);
    files[file] = NULL, file_sizes[file] = 0;
    unlock();
}

const RaceSoundBankOps *drive_audio_bank_ops(void)
{
    static const RaceSoundBankOps ops = { NULL, bank_unregister, bank_free };
    return &ops;
}

/* the mixing and the 100 Hz service run on the device's thread (mix): nothing is left for the game's loops to do */
void drive_audio_pump(void) {}

/* port (dev, env only): TNFS_AUDIO_DUMP=out.wav, in the headless --drive. The driver with no device, the banks loaded as a race loads them, and every tick the mixer
 * renders its share of frames into a WAV, so the sound as the race commands it (race_sound.c) is heard as the mixer makes it, tick for tick, as fast as the sim runs. The
 * sounds the race starts are logged (port_log: "audio: start sound ..."). */
static FILE *drive_dump_file;
static uint32_t drive_dump_frames;
static int32_t (*dump_real_start)(void *, int32_t, int32_t, uint8_t, uint8_t);

static int32_t dump_voice_start(void *user, int32_t sound, int32_t pan, uint8_t volume, uint8_t pitch)
{
    port_log("audio: start sound %#x pan %d volume %d pitch %d (tick %u)\n", (unsigned)sound, pan, volume, pitch, drive_dump_frames / (MIX_RATE / STEPS_PER_SECOND));
    return dump_real_start(user, sound, pan, volume, pitch);
}

static void dump_header(FILE *f, uint32_t frames)
{
    const uint32_t data = frames * 4, rate = MIX_RATE, byte_rate = rate * 4;
    const uint8_t h[44] = { 'R', 'I', 'F', 'F', (uint8_t)(36 + data), (uint8_t)((36 + data) >> 8), (uint8_t)((36 + data) >> 16), (uint8_t)((36 + data) >> 24),
        'W', 'A', 'V', 'E', 'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0, 2, 0, (uint8_t)rate, (uint8_t)(rate >> 8), (uint8_t)(rate >> 16), (uint8_t)(rate >> 24),
        (uint8_t)byte_rate, (uint8_t)(byte_rate >> 8), (uint8_t)(byte_rate >> 16), (uint8_t)(byte_rate >> 24), 4, 0, 16, 0,
        'd', 'a', 't', 'a', (uint8_t)data, (uint8_t)(data >> 8), (uint8_t)(data >> 16), (uint8_t)(data >> 24) };
    fwrite(h, 1, sizeof h, f);
}

int drive_audio_offline_open(RaceState *race, const char *data_root, const char *wav_path)
{
    root = data_root;
    drive_dump_file = fopen(wav_path, "wb");
    if (drive_dump_file == NULL) return 0;
    dump_header(drive_dump_file, 0);
    drive_dump_frames = 0, tick_frames = 0;
    sound_channels_reset(&driver.channels);
    driver.rand_seed = RAND_SEED;
    sound_driver_init(&driver, MIX_RATE);
    driver.channels.open = 1;
    open_ = 1, mixing = 0;
    RaceEntry *followed = &race->entries[race_slot(race->player)];
    followed->sound_volume = SOUND_VOLUME, followed->music_volume = 0, followed->sound_mode = 0, followed->sound_on = 1;
    race->unknown_52b30e = 1;
    race->sounds.voice_start = dump_voice_start;
    dump_real_start = voice_start;
    race->sounds.voice_volume = voice_volume;
    race->sounds.voice_pitch = voice_pitch;
    race->sounds.voice_pan = voice_pan;
    race->sounds.voice_stopped = voice_stopped;
    race->sounds.voice_stop = voice_stop;
    return 1;
}

void drive_audio_offline_tick(void)
{
    if (drive_dump_file == NULL) return;
    /* a race step is 1/30 s (drive_clock.h: 30 steps a second): that many frames, the 100 Hz service run every TICK_FRAMES of them as the device's thread does (mix) */
    int16_t frames[2 * TICK_FRAMES];
    for (uint32_t left = MIX_RATE / STEPS_PER_SECOND; left > 0;) {
        const uint32_t n = left < TICK_FRAMES ? left : TICK_FRAMES;
        sound_mix_frames(&driver.mixer, frames, (int32_t)n);
        thunder_mix(&thunder, frames, n);
        wet_roll_mix(&wet_roll, frames, n);
        wet_roll_mix(&rain_out, frames, n);
        wet_roll_mix(&rain_in, frames, n);
        wet_roll_mix(&snow_roll, frames, n);
        fwrite(frames, sizeof frames[0], 2 * n, drive_dump_file);
        drive_dump_frames += n, left -= n;
        for (tick_frames += n * TICKS_PER_SECOND; tick_frames >= MIX_RATE; tick_frames -= MIX_RATE) sound_driver_tick(&driver);
    }
}

void drive_audio_offline_close(void)
{
    if (drive_dump_file == NULL) return;
    fseek(drive_dump_file, 0, SEEK_SET);
    dump_header(drive_dump_file, drive_dump_frames);
    fclose(drive_dump_file);
    drive_dump_file = NULL;
}

enum { EACS_HEADER = 0x20 };

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

int32_t drive_audio_speech_setup(int32_t channel_a, int32_t channel_b, uint32_t volume)
{
    lock();
    const int32_t r = sound_queue_setup(&driver, &speech, channel_a, channel_b, volume);
    unlock();
    return r;
}

int32_t drive_audio_speech_add(const uint8_t *file, size_t size, uint32_t percent)
{
    if (file == NULL || size < EACS_HEADER) return -1;
    const uint8_t *h = file;
    const uint64_t bytes = (uint64_t)le32(h + 0xc) * h[8] * h[9];
    if (h[0xa] != 0 || bytes > size - EACS_HEADER) return -1;  /* the frames past the file: a cut or packed file */
    lock();
    SoundSample *s = &speech_sample[speech.write];
    *s = (SoundSample){ le32(h), le32(h + 4), h[8], h[9], h[0xa], (int8_t)h[0xb], le32(h + 0xc), le32(h + 0x10),
                        le32(h + 0x14), NULL, h[0x1c] };
    const int32_t r = sound_queue_add(&driver, &speech, s, file, percent);
    unlock();
    return r;
}

void drive_audio_speech_play(void) { lock(), sound_queue_play(&speech), unlock(); }

/* under the lock: once it returns the file is the caller's to free */
void drive_audio_speech_stop(void) { lock(), sound_queue_stop(&driver, &speech), unlock(); }

/* The music: the race's words, the reader, its ring and the file */
static RaceMusic music;
static MusicReader reader;
static uint8_t *music_ring;
static uint8_t *music_file;
static char music_path[512];
static uint32_t music_ticks_ms;

/* 00475c14(0): the sound kept going - the lock let go meanwhile, so the device's thread mixes and ticks (00464568
 * waits here for a fade to stop the music's voice) */
static void music_service(void *ctx)
{
    (void)ctx;
    const int depth = held;
    for (int i = 0; i < depth; i++) unlock();
    platform_sleep_ms(1);
    for (int i = 0; i < depth; i++) lock();
}

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
        port_log("drive_audio: music %s not loaded\n", music_path);
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

void drive_audio_music_play(const char *name) { lock(), race_music_play(&music_ops, &music, name), unlock(); }
void drive_audio_music_fill(void) { lock(), race_music_fill(&music_ops, &music), unlock(); }
void drive_audio_music_pump(void) { lock(), race_music_pump(&music_ops, &music), unlock(); }
void drive_audio_music_volume(int16_t volume) { lock(), race_music_volume(&music_ops, &music, volume), unlock(); }
void drive_audio_music_volume_now(int16_t volume)
{
    lock(), race_music_volume_now(&music_ops, &music, volume), unlock();
}
int16_t drive_audio_music_setting(void) { return music.setting; }
void drive_audio_music_small(int16_t small) { music.small = small; }

enum { MUSIC_FADE_STEPS = 100, MUSIC_FADE_TO_STOP = -1 };  /* 004644f0: 00473b00(handle, 100, -1) */

static int music_playing(void) { return music.unknown_52b31e == 0 && music.setting != 0 && music.sound_on != 0 && music.open != 0; }

void drive_audio_music_cut(void)
{
    lock();
    if (music_playing()) music_stop(NULL, music.handle);
    unlock();
}

void drive_audio_music_fade(void)
{
    lock();
    if (music_playing()) music_ramp(NULL, music.handle, MUSIC_FADE_STEPS, MUSIC_FADE_TO_STOP);
    unlock();
}

void drive_audio_music_stop(void) { lock(), race_music_stop(&music_ops, &music), unlock(); }

void drive_audio_close(void)
{
    lock();
    if (music.open) music_stop(NULL, music.handle), race_music_stop(&music_ops, &music);
    music_close(NULL, MUSIC_READER);
    sound_bank_unregister(&driver, SOUND_BANK_ALL);
    for (int i = 0; i < RACE_SOUND_BANKS; i++) bank_free(NULL, (uint32_t)i);
    /* the device can be opened again (the menus' around a race): nothing left playing from the files and the ring
     * freed above, no track open */
    for (int k = 0; k < SOUND_VOICES; k++) driver.voice[k].on = 0;
    driver.stream = (SoundStream){ 0 };
    speech = (SoundQueue){ 0 };
    music.open = 0, music.started = 0;
    mixing = 0;
    unlock();
    if (open_) platform_audio_close();
    open_ = 0;
}
