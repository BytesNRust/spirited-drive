/* A race with a cheat on and the same race replayed from its replay image, tick by tick: the cars must stand where
 * they stood. The replay image is the setup 004fb7bc (race_replay_image.h) the race writes at its end; the replay
 * starts from what that holds plus the cheat words it carries, and runs the inputs the race recorded.
 *
 * EXPLODE: the horn spins out the computer cars near the player (ai_manager_tick.c). The first replay leaves the cheat
 * out, as a replay did before the setup carried it: the cars must then part from the race (so the scene is one the
 * cheat decides). The second applies what the image holds: every car, every tick, the same.
 *   test_replay_cheats_sim SIMDATA_DIR */
#include "formats/tri.h"
#include "game/race/car_body.h"
#include "game/race/car_load.h"
#include "game/race/race_replay_image.h"
#include "game/race/race_sim.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    STEER_CENTRE = 0x20,
    THROTTLE = STEER_CENTRE,
    GEAR_UP_KEY = 0x1000,
    HORN_KEY = 0x4000,
    DRIVE_AT = 320,
    HORN_FROM = 330, HORN_TO = 700,
    TICKS = 900,
    SINGLE_RACE = 2,
    SEED = 0x2468ace
};

typedef struct {
    int32_t x[TICKS][RACE_PLAYERS], z[TICKS][RACE_PLAYERS];
} Trace;

static RaceSim sim;
static CarPdn pdns[RACE_PLAYERS];
static Trace race_trace, replay_trace, bare_trace, again_trace;

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

static uint32_t input_at(int tick)
{
    const int tap = tick >= DRIVE_AT && tick < DRIVE_AT + 8;
    return THROTTLE | (tap ? GEAR_UP_KEY : 0) | (tick >= HORN_FROM && tick < HORN_TO ? HORN_KEY : 0);
}

static void run(Trace *out)
{
    for (int t = 0; t < TICKS; t++) {
        race_sim_tick(&sim, input_at(sim.race.tick));
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
        fprintf(stderr, "usage: test_replay_cheats_sim SIMDATA_DIR\n");
        return 2;
    }
    char path[512];
    snprintf(path, sizeof path, "%s/MISC/TR1.TRI", argv[1]);
    size_t size = 0;
    uint8_t *tri_file = read_file(path, &size);
    Tri tri;
    if (tri_file == NULL || tri_open(&tri, tri_file, size) != TRI_OK) {
        fprintf(stderr, "test_replay_cheats_sim: cannot open %s\n", path);
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
            fprintf(stderr, "test_replay_cheats_sim: cannot read %s\n", path);
            free(tri_file);
            return 1;
        }
        field.pdns[i] = &pdns[i];
    }

    /* the race, EXPLODE on, and the image it leaves */
    race_sim_start_field(&sim, &tri, "TR1", &specs, &field, 2, 0);
    sim.race.unknown_4c5cd4 = 1;
    run(&race_trace);
    static uint8_t setup[REPLAY_SETUP_SIZE];
    race_replay_image_setup(&sim.race, SEED, setup);
    ReplayRace replay;
    if (!race_replay_image_race(setup, &replay)) {
        fprintf(stderr, "the race's setup does not read back as a race\n");
        free(tri_file);
        return 1;
    }

    /* the replay as it started before the setup carried the cheat, then with it */
    RaceField again = replay.field;
    memcpy(again.pdns, field.pdns, sizeof again.pdns);
    again.track_flags = replay.track_flags;
    race_sim_start_field(&sim, &tri, "TR1", &specs, &again, replay.laps, replay.arcade);
    run(&bare_trace);
    race_sim_start_field(&sim, &tri, "TR1", &specs, &again, replay.laps, replay.arcade);
    race_replay_apply_cheats(&sim.race, &replay);
    run(&replay_trace);
    race_sim_start_field(&sim, &tri, "TR1", &specs, &again, replay.laps, replay.arcade);   /* after the bare one: no order */
    race_replay_apply_cheats(&sim.race, &replay);
    run(&again_trace);
    free(tri_file);

    const int bare = parted(&race_trace, &bare_trace), kept = parted(&race_trace, &replay_trace);
    const int last = parted(&race_trace, &again_trace);
    printf("explode: the replay without the cheat parts at tick %d, with it at tick %d (again %d)\n", bare, kept, last);
    int ok = 1;
    if (bare < 0) {
        fprintf(stderr, "the scene does not depend on the cheat: no car parts without it\n");
        ok = 0;
    }
    if (kept >= 0 || last >= 0) {
        fprintf(stderr, "the replay with the cheat parts from the race at tick %d\n", kept);
        ok = 0;
    }
    return ok ? 0 : 1;
}
