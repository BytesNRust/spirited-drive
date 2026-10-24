/* A race with the NITRO start and the same race replayed from its replay image, tick by tick: the cars must stand where
 * they stood. The race holds the player's N through the start (start_keys[0]); the recording carries it as the launch
 * signal (race_replay_launch_word) and the replay takes it out again (race_replay_launch_split), as drive_replay.c does.
 * Replayed as the exe plays a replay (port_replay_launch 0) the player never launches, so the cars part from the race
 * (the scene is one the launch decides); replayed with the signal they stand where they stood.
 *   test_keyboard_race_sim SIMDATA_DIR */
/* The remapped keyboard in a race: the keys the race receives (controls.cfg's, through port_controls_apply_keys, as
 * drive_input_start sets them) read by the race's own input_read each tick into the word the sim takes. Holding Q with
 * the arrow keys' codes steers nothing; holding Q with Q as Steer left steers exactly as the arrow does with config.dat's
 * keys, tick by tick.
 *   test_keyboard_race_sim SIMDATA_DIR */
#include "formats/tri.h"
#include "game/race/car_body.h"
#include "game/race/car_load.h"
#include "game/race/input_setup.h"
#include "game/race/input_read.h"
#include "game/platform/platform.h"
#include "game/race/race_sim.h"
#include "game/sys/port_controls.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* port_controls.c reaches the platform's files; the test has none */
uint8_t *platform_read_user_file(const char *name, size_t *size) { (void)name, (void)size; return NULL; }
bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    (void)name, (void)data, (void)size;
    return false;
}
void platform_free_file(uint8_t *data) { (void)data; }

enum {
    SC_Q = 0x10, SC_LEFT = 0x4b, SC_UP = 0x48, SC_A = 0x1e,
    GEAR_FROM = 320, GEAR_TO = 328, STEER_FROM = 340,
    TICKS = 900,
    SINGLE_RACE = 2,
    SEED = 0x2468ace
};

typedef struct {
    int32_t x[TICKS][RACE_PLAYERS], z[TICKS][RACE_PLAYERS];
} Trace;

static RaceSim sim;
static CarPdn pdns[RACE_PLAYERS];
static Trace arrow_trace, remapped_trace, unmapped_trace;

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

static void no_pad_read(void *ctx, int32_t player, int16_t stand_in) { (void)ctx, (void)player, (void)stand_in; }

/* the race's tick with `held` down: the gas always, the gear tap, the steering key from STEER_FROM, read through the
 * codes the race has */
static void run(Trace *out, const int32_t *codes, int steer_key)
{
    static ControlSetup controls[RACE_PLAYERS];
    static RaceEntry entries[RACE_PLAYERS];
    int16_t stand_in[RACE_PLAYERS] = { 0 };
    for (int t = 0; t < TICKS; t++) {
        uint8_t keys[256] = { 0 };
        const int tick = sim.race.tick;
        keys[SC_UP] = 1;
        keys[SC_A] = tick >= GEAR_FROM && tick < GEAR_TO;
        keys[steer_key] = tick >= STEER_FROM;
        InputPad pad = { 0 };
        MouseSteerWords mouse = { 0 };
        const InputReadOps ops = { .pad_read = no_pad_read };
        const InputReadInputs in = { keys, codes, stand_in, controls, entries, 0, 0, 0 };
        race_sim_tick(&sim, input_read(&ops, &pad, &mouse, &in));
        for (int i = 0; i < RACE_PLAYERS; i++)
            out->x[t][i] = sim.race.cars[i]->x, out->z[t][i] = sim.race.cars[i]->z;
    }
}

/* the first tick the traces part at, or -1 */
static int parted(const Trace *a, const Trace *b)
{
    for (int t = 0; t < TICKS; t++)
        for (int i = 0; i < RACE_PLAYERS; i++)
            if (a->x[t][i] != b->x[t][i] || a->z[t][i] != b->z[t][i]) return t;
    return -1;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: test_keyboard_race_sim SIMDATA_DIR\n");
        return 2;
    }
    char path[512];
    snprintf(path, sizeof path, "%s/MISC/TR1.TRI", argv[1]);
    size_t size = 0;
    uint8_t *tri_file = read_file(path, &size);
    Tri tri;
    if (tri_file == NULL || tri_open(&tri, tri_file, size) != TRI_OK) {
        fprintf(stderr, "test_keyboard_race_sim: cannot open %s\n", path);
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
            fprintf(stderr, "test_keyboard_race_sim: cannot read %s\n", path);
            free(tri_file);
            return 1;
        }
        field.pdns[i] = &pdns[i];
    }

    /* config.dat's keys (the arrow steers left), then controls.cfg's with Q for Steer left through the file's own text */
    int32_t config_codes[INPUT_KEY_CODES] = { SC_LEFT, 0x4d, SC_UP, 0x50, SC_A, 0x2c, 0x39, 0x23, 0x2e, 0 };
    PortControls player = port_controls_defaults();
    const int defaults[PKEY_COUNT] = { SC_LEFT, 0x4d, SC_UP, 0x50, SC_A, 0x2c, 0x39, 0x23, 0x2e };
    port_controls_set_key(&player, defaults, PKEY_LEFT, SC_Q);
    char text[2048];
    const size_t text_size = port_controls_format(&player, text, sizeof text);
    const PortControls loaded = port_controls_parse(text, text_size);
    int32_t remapped_codes[INPUT_KEY_CODES];
    memcpy(remapped_codes, config_codes, sizeof remapped_codes);
    port_controls_apply_keys(&loaded, remapped_codes);

    race_sim_start_field(&sim, &tri, "TR1", &specs, &field, 2, 0);
    run(&arrow_trace, config_codes, SC_LEFT);
    race_sim_start_field(&sim, &tri, "TR1", &specs, &field, 2, 0);
    run(&remapped_trace, remapped_codes, SC_Q);
    race_sim_start_field(&sim, &tri, "TR1", &specs, &field, 2, 0);
    run(&unmapped_trace, config_codes, SC_Q);
    free(tri_file);

    const int remapped = parted(&arrow_trace, &remapped_trace), unmapped = parted(&arrow_trace, &unmapped_trace);
    printf("keyboard: Q as Steer left parts from the arrow's race at tick %d, Q with config.dat's keys at tick %d\n",
           remapped, unmapped);
    int ok = 1;
    if (unmapped < 0) {
        fprintf(stderr, "the scene does not depend on the key: the car steers the same without it\n");
        ok = 0;
    }
    if (remapped >= 0) {
        fprintf(stderr, "the remapped key does not steer as the arrow does (parts at tick %d)\n", remapped);
        ok = 0;
    }
    return ok ? 0 : 1;
}
