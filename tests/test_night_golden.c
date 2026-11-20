/* The night and the car's lights, pinned (dep59 car editor, N1): what the race does to a car's palette and lamp texels at night, on synthetic data (no game
 * data, so it runs in CI), before that code moves to the core: the tint of a car's palette (race_palette_tint_shape), the track INFO's tint for a track and a
 * time of day (track_info_load), the lamp texels' entries in each state (lamp_texels_write), the police lights' flash (phase, table, glow). Each result is hashed
 * (FNV-1a, 64 bits) and compared with the value this file holds. The code moves with no change in behaviour, so none of these may change with it: a changed
 * hash is a changed night, never a "fix" of the number. */
#include "game/race/car_mask.h"
#include "game/race/lamp_emissive.h"
#include "game/race/police_lights.h"
#include "game/race/race_palette.h"
#include "game/race/track_info.h"
#include "test.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static const uint64_t GOLDEN_TINT = 0xddb67217e6e48ab2ULL, GOLDEN_INFO = 0x3ece4548a8ba3c7dULL, GOLDEN_LAMPS = 0xd8f1ef27cfe30b33ULL, GOLDEN_POLICE = 0xea84341088aef9fcULL;

#define FNV_START 0xcbf29ce484222325ULL
static uint64_t fnv(uint64_t h, const void *data, size_t n)
{
    const uint8_t *p = data;
    for (size_t i = 0; i < n; i++) h = (h ^ p[i]) * 0x100000001b3ULL;
    return h;
}

static uint32_t lcg_state = 12345;
static uint32_t lcg(void)
{
    lcg_state = lcg_state * 1664525u + 1013904223u;
    return lcg_state >> 8;
}

static void report(const char *what, uint64_t got, uint64_t pinned)
{
    printf("golden %-8s 0x%016" PRIx64 "%s\n", what, got, got == pinned ? "" : "   <- NOT the pinned value");
    CHECK(got == pinned);
}

/* a car's palette shape: 256 colours of 6 bits, tinted in place by the race (004473d8) */
static void test_tint(void)
{
    static const int32_t TINTS[][5] = {   /* red, green, blue (8.8), saturation (16.16), brightness (16.16) */
        { 0x100, 0x100, 0x100, 0x10000, 0x10000 },   /* neutral */
        { 0xe0, 0xd0, 0xb0, 0x8000, 0xc000 },        /* dusk-like */
        { 0x50, 0x60, 0xa0, 0x4000, 0x6000 },        /* night-like */
        { 0x1c0, 0x80, 0x20, 0x20000, 0x14000 },     /* over-range: clamps and a saturation above 1 */
    };
    uint64_t h = FNV_START;
    for (size_t t = 0; t < sizeof TINTS / sizeof *TINTS; t++) {
        uint8_t shape[0x10 + 768] = { 0 };
        shape[0] = 0x24;
        shape[4] = 0x00, shape[5] = 0x03;   /* width 768 */
        shape[6] = 0x01, shape[7] = 0x00;   /* height 1: 256 colours */
        lcg_state = 777;
        for (int i = 0; i < 768; i++) shape[0x10 + i] = (uint8_t)(lcg() & 63);
        static RacePalette p;
        memset(&p, 0, sizeof p);
        race_palette_set_tint(&p, TINTS[t][0], TINTS[t][1], TINTS[t][2], TINTS[t][3], TINTS[t][4]);
        race_palette_tint_shape(&p, shape);
        h = fnv(h, shape + 0x10, 768);
    }
    report("tint", h, GOLDEN_TINT);
}

typedef struct {
    uint64_t hash;
    int tints;
} InfoSeen;

static int32_t info_open(void *ctx, const char *name) { (void)ctx, (void)name; return 0; }
static int info_gets(void *ctx, char *line, int32_t size, int32_t file) { (void)ctx, (void)line, (void)size, (void)file; return 0; }
static void info_close(void *ctx, int32_t file) { (void)ctx, (void)file; }
static void info_tint(void *ctx, int32_t red, int32_t green, int32_t blue, int32_t saturation, int32_t brightness)
{
    InfoSeen *s = ctx;
    const int32_t v[5] = { red, green, blue, saturation, brightness };
    s->hash = fnv(s->hash, v, sizeof v);
    s->tints++;
}

/* the INFO's tint of each track the table has, for each time of day (no file: the stem's row) */
static void test_info(void)
{
    static const char *const STEMS[] = { "cl1", "Cl3", "cy1", "tr1", "tr5", "TR7", "nope" };
    InfoSeen seen = { FNV_START, 0 };
    for (size_t s = 0; s < sizeof STEMS / sizeof *STEMS; s++)
        for (int tod = 0; tod < 3; tod++) {
            TrackInfoOps ops = { &seen, info_open, info_gets, info_close, info_tint };
            TrackInfoInputs in = { STEMS[s], "", 0, tod, 1 };
            TrackInfo info;
            memset(&info, 0, sizeof info);
            track_info_load(&ops, &in, &info);
            seen.hash = fnv(seen.hash, &info, sizeof info);
        }
    CHECK(seen.tints > 0);
    report("info", seen.hash, GOLDEN_INFO);
}

/* the lamp texels of a synthetic mask in every state */
static void test_lamps(void)
{
    uint8_t mask[64];
    static const uint8_t CLASS[] = { CAR_MASK_PAINT, CAR_MASK_BRAKE, CAR_MASK_MATTE, CAR_MASK_POSITION, CAR_MASK_HEADLIGHT, CAR_MASK_BLACK, CAR_MASK_SHINE, CAR_MASK_REVERSE };
    lcg_state = 4242;
    for (int i = 0; i < 64; i++) mask[i] = CLASS[lcg() % (sizeof CLASS)];
    LampTexels l = { NULL, 0, 0 };
    CHECK(lamp_texels_collect(&l, 8, mask, 64) > 0);
    uint8_t file[96];
    for (int i = 0; i < 96; i++) file[i] = (uint8_t)(100 + i);
    lamp_texels_read(&l, file, sizeof file);
    const LampEntries e = { 0xfe, 0xf0, 0xf1, 0xf2 };
    static const int STATES[] = { LAMP_DAY, LAMP_LOW, LAMP_BRAKING, LAMP_DAY | LAMP_REVERSING, LAMP_LOW | LAMP_REVERSING, LAMP_BRAKING | LAMP_REVERSING };
    uint64_t h = fnv(FNV_START, &l.count, sizeof l.count);
    for (size_t s = 0; s < sizeof STATES / sizeof *STATES; s++) {
        const int changed = lamp_texels_write(&l, file, sizeof file, (LampState)STATES[s], &e);
        h = fnv(fnv(h, &changed, sizeof changed), file, sizeof file);
    }
    const float night[3] = { 0.5f, 0.5f, 0.6f }, day[3] = { 1, 1, 1 }, tunnel[3] = { 0.6f, 0.6f, 0.6f };
    const int low[4] = { lamp_emissive_low(night, day), lamp_emissive_low(day, day), lamp_emissive_low(day, tunnel), lamp_emissive_low(NULL, NULL) };
    h = fnv(h, low, sizeof low);
    LampLight s = { 0, 0 };
    for (int f = 0; f < 40; f++) {
        const float ratio[3] = { f < 5 ? 0.6f : 0.9f, 0.9f, 0.9f };
        const int on = lamp_light_step(&s, day, ratio);
        h = fnv(h, &on, sizeof on);
    }
    lamp_texels_free(&l);
    report("lamps", h, GOLDEN_LAMPS);
}

/* the police car's flash: the phase over a few cycles, the nearest entries, the table in each phase, the glow */
static void test_police(void)
{
    uint8_t palette[768];
    lcg_state = 99;
    for (int i = 0; i < 768; i++) palette[i] = (uint8_t)lcg();
    uint64_t h = FNV_START;
    for (int32_t t = 0; t < 70; t++) {
        const int p = police_lights_phase(t);
        h = fnv(h, &p, sizeof p);
    }
    const int red = police_lights_entry(palette, 1), blue = police_lights_entry(palette, 0);
    h = fnv(fnv(h, &red, sizeof red), &blue, sizeof blue);
    uint8_t table[256];
    for (int32_t tick = 0; tick < 40; tick += 15) {
        police_lights_table(NULL, palette, tick, table);
        h = fnv(h, table, sizeof table);
    }
    const uint8_t found[4] = { 0xc0, 0xc1, 0xc2, 0xc3 };
    police_lights_set_found(found, 4);
    uint8_t base[256];
    for (int i = 0; i < 256; i++) base[i] = (uint8_t)(255 - i);
    for (int32_t tick = 0; tick < 40; tick += 15) {
        police_lights_table(base, palette, tick, table);
        h = fnv(h, table, sizeof table);
    }
    const int own[4] = { police_lights_own_entry(1), police_lights_own_entry(0), police_lights_own_centre(1), police_lights_own_centre(0) };
    h = fnv(h, own, sizeof own);
    police_lights_set_found(found, 0);
    for (int r = 0; r < 2; r++) {
        uint8_t rgb[3];
        police_glow_core(r, rgb);
        h = fnv(h, rgb, 3);
        for (int low = 0; low < 2; low++)
            for (int level = 0; level < 3; level++) {
                const int pc = police_glow_percent(low, level);
                h = fnv(h, &pc, sizeof pc);
            }
    }
    report("police", h, GOLDEN_POLICE);
}

int main(void)
{
    test_tint();
    test_info();
    test_lamps();
    test_police();
    return TEST_REPORT();
}
