/* The race's sound levels (race_sound*.c) as a trace: a race of six cars on TR1 run for TICKS ticks with the sound driver's calls (race->sounds) hashed, with the player's
 * sound on, then the music picks, the mute / unmute calls and the pan and loudness curves hashed too. The hash is the one the code gave before race_sound.c was split by
 * responsibility; a change to what the sound code does changes it (update EXPECTED with the reason in the commit, as with the goldens).
 *   test_race_sound_trace SIMDATA_DIR */
#include "formats/tri.h"
#include "game/race/car_body.h"
#include "game/race/car_load.h"
#include "game/race/race_sim.h"
#include "game/race/race_sound.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { TICKS = 1500, SINGLE_RACE = 2, SEED = 0x2468ace, THROTTLE = 0x20, GEAR_UP_KEY = 0x1000, GEAR_FROM = 320, GEAR_TO = 328 };

#define EXPECTED_HASH 0xbf663660dec2a766ull   /* the trace of the code before the split */

typedef struct {
    uint64_t hash;
    unsigned calls[12];
} Trace;

static void note(Trace *t, int op, int32_t a, int32_t b, int32_t c, int32_t d)
{
    const int32_t v[5] = { op, a, b, c, d };
    for (int i = 0; i < 5; i++)
        for (int k = 0; k < 4; k++) t->hash = (t->hash ^ (uint32_t)(v[i] >> (8 * k) & 0xff)) * 0x100000001b3ull;
    t->calls[op]++;
}

static int32_t t_start(void *u, int32_t sound, int32_t pan, uint8_t volume, uint8_t pitch)
{
    Trace *t = u;
    note(t, 0, sound, pan, volume, pitch);
    return (int32_t)(t->calls[0] % 16);   /* a voice number to hand back */
}
static void t_volume(void *u, int32_t voice, int32_t volume) { note(u, 1, voice, volume, 0, 0); }
static void t_pitch(void *u, int32_t voice, int32_t pitch) { note(u, 2, voice, pitch, 0, 0); }
static void t_pan(void *u, int32_t voice, int32_t pan) { note(u, 3, voice, pan, 0, 0); }
static int32_t t_stopped(void *u, int32_t voice)
{
    Trace *t = u;
    note(t, 4, voice, 0, 0, 0);
    return (voice + (int32_t)t->calls[4]) % 5 == 0;   /* some voices end */
}
static void t_stop(void *u, int32_t voice) { note(u, 5, voice, 0, 0, 0); }
static void t_music_volume(void *u, int16_t volume) { note(u, 6, volume, 0, 0, 0); }
static int32_t m_rand(void *u, int32_t n) { note(u, 7, n, 0, 0, 0); return (int32_t)(((Trace *)u)->calls[7] * 7 % (unsigned)n); }
static void m_play(void *u, int32_t track) { note(u, 8, track, 0, 0, 0); }

static RaceSim sim;
static CarPdn pdns[RACE_PLAYERS];
static Trace trace;

static uint8_t *read_file(const char *path, size_t *size)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) return NULL;
    uint8_t *data = NULL;
    long length = fseek(fp, 0, SEEK_END) == 0 ? ftell(fp) : -1;
    if (length > 0 && fseek(fp, 0, SEEK_SET) == 0 && (data = malloc((size_t)length)) != NULL &&
        fread(data, 1, (size_t)length, fp) != (size_t)length) {
        free(data);
        data = NULL;
    }
    fclose(fp);
    *size = data != NULL ? (size_t)length : 0;
    return data;
}

static void load_name(void *u, const char *name) { int32_t h = 0; for (const char *c = name; *c != '\0'; c++) h = h * 31 + *c; note(u, 5, h, 0, 0, 0); }
static void bank_unregister(void *u, uint32_t bank) { note(u, 5, (int32_t)bank, 1, 0, 0); }
static void bank_free(void *u, uint32_t file) { note(u, 5, (int32_t)file, 2, 0, 0); }

/* the calls the race loop does not make on this scene, with arguments over their ranges, on the race as it stands */
static void direct(RaceState *race)
{
    for (int32_t channel = 0; channel < 16; channel++)
        for (int32_t k = 0; k < 6; k++) {
            race_sound_channel(race, channel, 0x20 + k * 9, 0x10 + k * 13, 0x70 - k * 11, 0x30 + k * 17);
            race_sound_channel_at(race, channel, 0x21 + k * 7, 0x18 + k * 11, 0x60 - k * 9, k * 0x500000 - 0x800000);
            race_sound_effect(race, channel, 0x22 + k, 0x40 + k, 0x30 + k, k * 0x100000, 0x20);
            race_sound_effect_scaled(race, channel, 0x23 + k, 0x40 + k, 0x30 + k, k * 0x100000, 0x20);
            race_sound_play(race, channel, k, 0x40 + k, k * 0x40000, k * 0x180000, k * 0x600000 - 0x1000000);
            note(&trace, 9, race_sound_held_pitch(race, channel, 0x24 + k, k * 0x30000), 0, 0, 0);
            note(&trace, 9, race_sound_pick(race, channel, k, 0x10 * k), 0, 0, 0);
            race_sound_channel_stop(race, channel);
        }
    for (int32_t speed = 0; speed < 0x400000; speed += 0x31000)
        for (int32_t b = 0; b < 8; b++) note(&trace, 9, race_sound_pick_any(speed, b, 0x20 + b), 0, 0, 0);
    for (int i = 0; i < 12; i++) race_sound_pulse(race);
    for (int32_t distance = 0; distance < 0x300000; distance += 0x60000) {
        race_sound_signal(race, distance, distance / 2);
        race_sound_signal_off(race);
    }
    for (int i = 0; i + 1 < RACE_PLAYERS; i++) {
        note(&trace, 9, race_sound_speed_level(race->cars[i], race->cars[i + 1]), 0, 0, 0);
        int32_t angle = 0;
        race_sound_angle(race, race->cars[i], &angle);
        note(&trace, 9, angle, 0, 0, 0);
    }
    race_sound_other_human(race);
    race_sound_stop_engine(race);
    race_sound_start_engine(race);
    race_sound_setup(race, t_music_volume, &trace);
    race_sound_load_banks(race, load_name, &trace);
    RaceSoundBanks banks = { { 1, 2, 3, 4, 5, 6 }, { 11, 12, 13, 14, 15, 16 }, 8 };
    const RaceSoundBankOps ops = { &trace, bank_unregister, bank_free };
    race_sound_end(race, &banks, &ops);
    note(&trace, 9, banks.count, 0, 0, 0);
}

static void curves(void)
{
    for (int32_t angle = -0x2000000; angle < 0x2000000; angle += 0x1000) note(&trace, 9, race_sound_pan(angle), 0, 0, 0);
    for (int32_t distance = 0; distance < 0x400000; distance += 0x400) note(&trace, 10, race_sound_loudness(distance), 0, 0, 0);
}

static void music(void)
{
    const RaceMusicOps ops = { &trace, m_rand, m_play };
    int16_t small = 0;
    for (int16_t setting = -1; setting <= 16; setting++) {
        race_music_start(&ops, setting);
        race_music_pick(&ops, setting, &small);
        note(&trace, 11, small, setting, 0, 0);
        for (int32_t idle = -3; idle < 40; idle += 7) {
            race_music_by_idle(&ops, setting, idle);
            race_music_menu_pick(&ops, setting, &small, idle & 1);
            race_music_menu_by_idle(&ops, setting, idle, idle & 1);
        }
    }
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: test_race_sound_trace SIMDATA_DIR\n");
        return 2;
    }
    char path[512];
    snprintf(path, sizeof path, "%s/MISC/TR1.TRI", argv[1]);
    size_t size = 0;
    uint8_t *tri_file = read_file(path, &size);
    Tri tri;
    if (tri_file == NULL || tri_open(&tri, tri_file, size) != TRI_OK) {
        fprintf(stderr, "test_race_sound_trace: cannot open %s\n", path);
        free(tri_file);
        return 1;
    }
    snprintf(path, sizeof path, "%s/CARSPECS/%s.PBS", argv[1], car_model_names[0]);
    uint8_t *file = read_file(path, &size);
    CarSpecs specs;
    const int read = file != NULL && car_specs_read(&specs, file, size);
    free(file);
    if (!read) {
        printf("skip (no %s)\n", path);
        free(tri_file);
        return 0;
    }
    RaceField field;
    memset(&field, 0, sizeof field);
    field.racers = RACE_PLAYERS;
    field.race_type = SINGLE_RACE;
    field.seed = SEED;
    for (int i = 0; i < RACE_PLAYERS; i++) {
        snprintf(field.entries[i].name, sizeof field.entries[i].name, "Car%d", i);
        field.entries[i].car = i;
        field.entries[i].automatic = 1;
        snprintf(path, sizeof path, "%s/CARFAMS/%s.PDN", argv[1], car_model_names[i]);
        uint8_t *f = read_file(path, &size);
        const int ok = f != NULL && car_pdn_read(&pdns[i], f, size);
        free(f);
        if (!ok) {
            fprintf(stderr, "test_race_sound_trace: cannot read %s\n", path);
            free(tri_file);
            return 1;
        }
        field.pdns[i] = &pdns[i];
    }
    race_sim_start_field(&sim, &tri, "TR1", &specs, &field, 2, 0);
    RaceState *race = &sim.race;
    race->sounds.user = &trace;
    race->sounds.voice_start = t_start, race->sounds.voice_volume = t_volume, race->sounds.voice_pitch = t_pitch;
    race->sounds.voice_pan = t_pan, race->sounds.voice_stopped = t_stopped, race->sounds.voice_stop = t_stop;
    for (int i = 0; i < RACE_PLAYERS; i++) race->entries[i].sound_volume = 0x7f, race->entries[i].music_volume = 0x60;
    race_sound_start_voices(race);
    race_sound_start_engine(race);
    for (int t = 0; t < TICKS; t++) {
        race_sim_tick(&sim, THROTTLE | (t >= GEAR_FROM && t < GEAR_TO ? GEAR_UP_KEY : 0));
        if (t % 300 == 299) {   /* the pause, the mute and the end of a voice's life, as the drive calls them */
            race_sound_mute(race, t_music_volume, &trace);
            race_sound_unmute(race, t_music_volume, &trace);
            race_sound_voices_off(race, t_music_volume, &trace);
            race_sound_music_back(race, t_music_volume, &trace);
            race_sound_start_voices(race);
        }
    }
    direct(race);
    race_sound_channels_stop(race);
    curves();
    music();
    free(tri_file);
    printf("race sound trace: hash 0x%016" PRIx64 ", calls", trace.hash);
    for (int i = 0; i < 12; i++) printf(" %u", trace.calls[i]);
    printf("\n");
    if (EXPECTED_HASH != 0 && trace.hash != EXPECTED_HASH) {
        fprintf(stderr, "the race sound trace changed: expected 0x%016llx\n", (unsigned long long)EXPECTED_HASH);
        return 1;
    }
    return 0;
}
