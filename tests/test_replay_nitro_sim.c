/* A race with the NITRO start and the same race replayed from its replay image, tick by tick: the cars must stand where
 * they stood. The race holds the player's N through the start (start_keys[0]); the recording carries it as the launch
 * signal (race_replay_launch_word) and the replay takes it out again (race_replay_launch_split), as drive_replay.c does.
 * Replayed as the exe plays a replay (port_replay_launch 0) the player never launches, so the cars part from the race
 * (the scene is one the launch decides); replayed with the signal they stand where they stood.
 *   test_replay_nitro_sim SIMDATA_DIR */
#include "formats/tri.h"
#include "game/race/car_body.h"
#include "game/race/car_load.h"
#include "game/race/race_replay.h"
#include "game/race/race_replay_image.h"
#include "game/race/race_sim.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    STEER_CENTRE = 0x20,
    THROTTLE = STEER_CENTRE,
    GEAR_UP_KEY = 0x1000, GEAR_FROM = 320, GEAR_TO = 328,   /* out of neutral, into the gear the launch drives */
    N_FROM = 330, N_TO = 700,
    RECORDING = 1 << 17,
    TICKS = 900,
    SINGLE_RACE = 2,
    SEED = 0x2468ace
};

typedef struct {
    int32_t x[TICKS][RACE_PLAYERS], z[TICKS][RACE_PLAYERS];
} Trace;

static RaceSim sim;
static CarPdn pdns[RACE_PLAYERS];
static Trace race_trace, replay_trace, bare_trace;
static uint8_t recording[RECORDING];

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

/* the race: N held from N_FROM to N_TO, recorded as the drive does each step; the replay: played back each step */
static void run(Trace *out, int replaying, int launch_port)
{
    sim.race.port_replay_launch = launch_port;
    sim.race.port_nitro_start = 1;   /* as track_viewer.c sets it, replay or not */
    for (int t = 0; t < TICKS; t++) {
        RaceState *race = &sim.race;
        race->start_clock = race->tick;   /* the timer the viewer fills (start_clock_fill), one a tick */
        const RaceReplayWords w = { 1, 1, 1, race->clock };
        uint32_t word = THROTTLE | (race->tick >= GEAR_FROM && race->tick < GEAR_TO ? GEAR_UP_KEY : 0);
        if (replaying) {
            uint32_t in[RACE_PLAYERS] = { 0 };
            race_replay_play(&w, in, recording, RECORDING);
            word = in[0];   /* the recording has the gear tap, the signal when N was held */
            race->start_keys[0] = 0;
            if (race->port_replay_launch != 0 && race->start_input != 0)
                race->start_keys[0] = (uint8_t)race_replay_launch_split(&word);
        } else {
            race->start_keys[0] = (uint8_t)(race->tick >= N_FROM && race->tick < N_TO);
            const RaceReplayWords rw = { 1, 0, 1, race->clock };
            uint32_t in[RACE_PLAYERS] = { race_replay_launch_word(word, race->start_keys[0] != 0) };
            race_replay_record(&rw, in, recording, RECORDING);
        }
        race_sim_tick(&sim, word);
        for (int i = 0; i < RACE_PLAYERS; i++)
            out->x[t][i] = race->cars[i]->x, out->z[t][i] = race->cars[i]->z;
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
        fprintf(stderr, "usage: test_replay_nitro_sim SIMDATA_DIR\n");
        return 2;
    }
    char path[512];
    snprintf(path, sizeof path, "%s/MISC/TR1.TRI", argv[1]);
    size_t size = 0;
    uint8_t *tri_file = read_file(path, &size);
    Tri tri;
    if (tri_file == NULL || tri_open(&tri, tri_file, size) != TRI_OK) {
        fprintf(stderr, "test_replay_nitro_sim: cannot open %s\n", path);
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
            fprintf(stderr, "test_replay_nitro_sim: cannot read %s\n", path);
            free(tri_file);
            return 1;
        }
        field.pdns[i] = &pdns[i];
    }

    /* the race, the NITRO start on, and the image it leaves */
    race_sim_start_field(&sim, &tri, "TR1", &specs, &field, 2, 0);
    sim.race.start_input = 1;
    run(&race_trace, 0, 1);
    static uint8_t setup[REPLAY_SETUP_SIZE];
    race_replay_image_setup(&sim.race, SEED, setup);
    ReplayRace replay;
    if (!race_replay_image_race(setup, &replay) || !replay.nitro) {
        fprintf(stderr, "the race's setup does not read back as a race with the NITRO start\n");
        free(tri_file);
        return 1;
    }

    /* the replay as the exe plays it (no launch), then with the signal */
    RaceField again = replay.field;
    memcpy(again.pdns, field.pdns, sizeof again.pdns);
    again.track_flags = replay.track_flags;
    race_sim_start_field(&sim, &tri, "TR1", &specs, &again, replay.laps, replay.arcade);
    race_replay_apply_cheats(&sim.race, &replay);
    run(&bare_trace, 1, 0);
    race_sim_start_field(&sim, &tri, "TR1", &specs, &again, replay.laps, replay.arcade);
    race_replay_apply_cheats(&sim.race, &replay);
    run(&replay_trace, 1, 1);
    free(tri_file);

    const int bare = parted(&race_trace, &bare_trace), kept = parted(&race_trace, &replay_trace);
    printf("nitro: the replay without the signal parts at tick %d, with it at tick %d\n", bare, kept);
    int ok = 1;
    if (bare < 0) {
        fprintf(stderr, "the scene does not depend on the launch: no car parts without it\n");
        ok = 0;
    }
    if (kept >= 0) {
        fprintf(stderr, "the replay with the signal parts from the race at tick %d\n", kept);
        ok = 0;
    }
    return ok ? 0 : 1;
}
