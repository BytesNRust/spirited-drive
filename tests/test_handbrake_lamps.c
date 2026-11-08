/* The handbrake and the brake lamps (handbrake_lamps): in the rally (the arcade variant) the exe turns the handbrake into a full brake (car_physics.c), which the brake lamps then show; the race
 * marks that brake (RaceState.port_hb_lamp) so the lamps can leave it alone. The mark is the handbrake's brake alone: set while the handbrake is held with no pedal brake behind it, clear when the
 * pedal brakes, clear once the brake has died away after the release, never set outside the rally. The physics do not read it (the exe's brake is still 0xfa).
 *   test_handbrake_lamps SIMDATA_DIR */
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

enum { SC_UP = 0x48, SC_DOWN = 0x50, SC_SPACE = 0x39, TICKS = 420, HOLD_FROM = 120, HOLD_TO = 260, SINGLE_RACE = 2, SEED = 0x2468ace };

static RaceSim sim;
static CarPdn pdns[RACE_PLAYERS];

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

typedef struct { int hb_held_marked, hb_held_total, hb_brake_total, marked_outside, marked_after_release, marked_with_pedal, with_pedal_total, brake_exe_ok; } Result;

/* one race: the gas the whole way, the handbrake from HOLD_FROM to HOLD_TO, and (pedal) the brake key as well from 180 */
static Result run(int arcade, int pedal, const RaceField *field, const Tri *tri, const CarSpecs *specs, const int32_t *codes)
{
    static ControlSetup controls[RACE_PLAYERS];
    static RaceEntry entries[RACE_PLAYERS];
    int16_t stand_in[RACE_PLAYERS] = { 0 };
    Result r = { 0 };
    race_sim_start_field(&sim, tri, "TR1", specs, field, 2, arcade);
    for (int t = 0; t < TICKS; t++) {
        uint8_t keys[256] = { 0 };
        const int tick = sim.race.tick;
        keys[SC_UP] = 1;
        keys[SC_SPACE] = tick >= HOLD_FROM && tick < HOLD_TO;
        keys[SC_DOWN] = pedal && tick >= 180 && tick < HOLD_TO;
        InputPad pad = { 0 };
        MouseSteerWords mouse = { 0 };
        const InputReadOps ops = { .pad_read = no_pad_read };
        const InputReadInputs in = { keys, codes, stand_in, controls, entries, 0, 0, 0, 0 };
        race_sim_tick(&sim, input_read(&ops, &pad, &mouse, &in));
        const Car *car = sim.race.cars[0];
        const int marked = sim.race.port_hb_lamp[car->index] != 0;
        const int held = tick >= HOLD_FROM + 2 && tick < HOLD_TO;
        if (held && !pedal) {
            r.hb_held_total++, r.hb_held_marked += marked;
            r.hb_brake_total += car->brake > 10;
            r.brake_exe_ok += car->brake == 0xfa;
        }
        if (held && pedal && tick >= 190) r.with_pedal_total++, r.marked_with_pedal += marked;
        if (tick < HOLD_FROM && marked) r.marked_outside++;
        if (tick >= HOLD_TO + 80 && marked) r.marked_after_release++;
    }
    return r;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: test_handbrake_lamps SIMDATA_DIR\n");
        return 2;
    }
    char path[512];
    snprintf(path, sizeof path, "%s/MISC/TR1.TRI", argv[1]);
    size_t size = 0;
    uint8_t *tri_file = read_file(path, &size);
    Tri tri;
    if (tri_file == NULL || tri_open(&tri, tri_file, size) != TRI_OK) {
        fprintf(stderr, "test_handbrake_lamps: cannot open %s\n", path);
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
            fprintf(stderr, "test_handbrake_lamps: cannot read %s\n", path);
            free(tri_file);
            return 1;
        }
        field.pdns[i] = &pdns[i];
    }
    int32_t codes[INPUT_KEY_CODES] = { 0x4b, 0x4d, SC_UP, SC_DOWN, 0x1e, 0x2c, SC_SPACE, 0x23, 0x2e, 0 };
    const Result rally = run(1, 0, &field, &tri, &specs, codes);
    const Result rally_pedal = run(1, 1, &field, &tri, &specs, codes);
    const Result road = run(0, 0, &field, &tri, &specs, codes);
    free(tri_file);
    printf("rally: handbrake ticks %d, marked %d, brake>10 %d, brake 0xfa %d, marked before %d, after release %d; with the pedal: %d of %d marked; road: marked %d\n",
           rally.hb_held_total, rally.hb_held_marked, rally.hb_brake_total, rally.brake_exe_ok, rally.marked_outside, rally.marked_after_release,
           rally_pedal.marked_with_pedal, rally_pedal.with_pedal_total, road.hb_held_marked);
    int ok = 1;
    if (rally.hb_held_total < 30 || rally.hb_held_marked != rally.hb_held_total) { fprintf(stderr, "the rally's handbrake brake is not marked on every tick\n"); ok = 0; }
    if (rally.hb_brake_total == 0 || rally.brake_exe_ok != rally.hb_held_total) { fprintf(stderr, "the exe's physics changed: the handbrake is not the full brake\n"); ok = 0; }
    if (rally.marked_outside != 0 || rally.marked_after_release != 0) { fprintf(stderr, "the mark outside the handbrake's time\n"); ok = 0; }
    if (rally_pedal.with_pedal_total < 20 || rally_pedal.marked_with_pedal != 0) { fprintf(stderr, "the pedal's brake is marked as the handbrake's\n"); ok = 0; }
    if (road.hb_held_marked != 0) { fprintf(stderr, "marked outside the rally\n"); ok = 0; }
    return ok ? 0 : 1;
}
