/* Head to head on a circuit run the other way round (Vertigo Ridge, TR3, port: formats/tri_reverse.h): the player and the
 * rival start a lane apart, as they do on the forward track, with the same separation across the road.
 *   test_reverse_h2h_grid SIMDATA_DIR */
#include "formats/tri.h"
#include "formats/tri_reverse.h"
#include "game/race/car_load.h"
#include "game/race/race_sim.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    HEAD_TO_HEAD = 1,
    RACERS = 2,
    TICKS = 3,
    LANE = 0x50000,             /* 5 units, 16.16: the TR3 grid's lane (ai_grid_place's gap) */
    SEPARATION_TOLERANCE = 8,   /* the reversed separation is within 1/8 of the forward one */
    MAX_HOVER = 0x20000         /* the player starts within 2 units of the road: not above it, to fall onto it */
};

static RaceSim forward_sim;
static RaceSim reverse_sim;
static CarPdn pdns[RACERS];
static int32_t hover[2];   /* the player's height over the road after the first ticks, forward and reversed */

static uint8_t *read_file(const char *path, size_t *size)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) return NULL;
    uint8_t *data = NULL;
    const long length = fseek(fp, 0, SEEK_END) == 0 ? ftell(fp) : -1;
    if (length > 0 && fseek(fp, 0, SEEK_SET) == 0 && (data = malloc((size_t)length)) != NULL &&
        fread(data, 1, (size_t)length, fp) != (size_t)length) {
        free(data);
        data = NULL;
    }
    fclose(fp);
    *size = data != NULL ? (size_t)length : 0;
    return data;
}

static int load_pdn(CarPdn *pdn, const char *simdata, int model)
{
    char path[512];
    snprintf(path, sizeof path, "%s/CARFAMS/%s.PDN", simdata, car_model_names[model]);
    size_t size = 0;
    uint8_t *file = read_file(path, &size);
    const int ok = file != NULL && car_pdn_read(pdn, file, size);
    free(file);
    if (!ok) fprintf(stderr, "test_reverse_h2h_grid: cannot read %s\n", path);
    return ok;
}

/* The distance across the road between the two cars once they are placed (road_lateral, 16.16). */
static int32_t separation(const RaceSim *sim)
{
    const int32_t player = sim->race.cars[0]->road_lateral, rival = sim->race.cars[1]->road_lateral;
    return player > rival ? player - rival : rival - player;
}

/* Starts the head-to-head TR3 race on `sim` (reversed or not) and returns the cars' separation after a few ticks. */
static int32_t start_head_to_head(RaceSim *sim, const uint8_t *tri_file, size_t size, const CarSpecs *specs,
                                  RaceField *field, int reversed)
{
    Tri tri;
    if (tri_open(&tri, tri_file, size) != TRI_OK) return -1;
    field->port_reverse = reversed;
    race_sim_start_field(sim, &tri, "TR3", specs, field, 2, 0);
    for (int t = 0; t < TICKS; t++) race_sim_tick(sim, 0);
    const Car *player = sim->race.cars[0];
    hover[reversed != 0] = player->y - player->road_point[1];
    return separation(sim);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: test_reverse_h2h_grid SIMDATA_DIR\n");
        return 2;
    }
    char path[512];
    snprintf(path, sizeof path, "%s/MISC/TR3.TRI", argv[1]);
    size_t size = 0;
    uint8_t *forward = read_file(path, &size);
    uint8_t *reversed = forward != NULL ? malloc(size) : NULL;
    if (reversed == NULL || tri_reverse(forward, size, reversed) != TRI_OK) {
        fprintf(stderr, "test_reverse_h2h_grid: cannot reverse %s\n", path);
        return 1;
    }
    snprintf(path, sizeof path, "%s/CARSPECS/%s.PBS", argv[1], car_model_names[0]);
    size_t specs_size = 0;
    uint8_t *specs_file = read_file(path, &specs_size);
    CarSpecs specs;
    const int read = specs_file != NULL && car_specs_read(&specs, specs_file, specs_size);
    free(specs_file);
    if (!read) {
        fprintf(stderr, "test_reverse_h2h_grid: cannot read %s\n", path);
        return 1;
    }
    RaceField field;
    memset(&field, 0, sizeof field);
    field.racers = RACERS;
    field.race_type = HEAD_TO_HEAD;
    for (int i = 0; i < RACERS; i++) {
        snprintf(field.entries[i].name, sizeof field.entries[i].name, "Car%d", i);
        field.entries[i].car = i;
        field.entries[i].automatic = 1;
        if (!load_pdn(&pdns[i], argv[1], i)) return 1;
        field.pdns[i] = &pdns[i];
    }
    const int32_t fwd = start_head_to_head(&forward_sim, forward, size, &specs, &field, 0);
    const int32_t rev = start_head_to_head(&reverse_sim, reversed, size, &specs, &field, 1);
    const int32_t diff = rev > fwd ? rev - fwd : fwd - rev;
    const int32_t up = hover[1] < 0 ? -hover[1] : hover[1];
    const int ok = fwd >= LANE && rev >= LANE && diff * SEPARATION_TOLERANCE <= fwd && up <= MAX_HOVER;
    printf("TR3 head to head: forward separation %d, reversed %d, lane %d; reversed start %d over the road%s\n", fwd, rev, LANE, hover[1], ok ? "" : "  FAIL");
    free(forward);
    free(reversed);
    return ok ? 0 : 1;
}
