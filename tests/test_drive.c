/* Drives each car down a track headless: an automatic put into drive after
 * the start, full throttle, the wheel straight. The car must get going and
 * travel along the road (checks the race loop holds together; the parts are
 * checked one by one against the original by tools/oracle_car.py).
 *   test_drive SIMDATA_DIR [TRACK] */
#include "formats/tri.h"
#include "game/race/car_load.h"
#include "game/race/race_sim.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    STEER_CENTRE = 0x20,
    THROTTLE = STEER_CENTRE,        /* pedal field 0: full throttle */
    GEAR_UP_KEY = 0x1000,           /* automatic: N to D (down is R) */
    DRIVE_AT = 320,                 /* race ticks: the gear keys work from 300 */
    SECONDS = 40
};

static RaceSim sim;

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

static int drive(const Tri *tri, const char *track, const char *simdata, int model)
{
    char path[512];
    snprintf(path, sizeof path, "%s/CARSPECS/%s.PBS", simdata, car_model_names[model]);
    size_t size = 0;
    uint8_t *file = read_file(path, &size);
    CarSpecs specs;
    const int read = file != NULL && car_specs_read(&specs, file, size);
    free(file);
    if (!read) {
        printf("skip %s (no %s)\n", car_model_names[model], path);
        return 1;
    }
    RaceEntry entry;
    memset(&entry, 0, sizeof entry);
    strcpy(entry.name, "Test");
    entry.car = model;
    entry.automatic = 1;
    race_sim_start(&sim, tri, track, &specs, &entry, 2, 0);
    const Car *car = sim.race.cars[0];
    const int32_t start = car->progress;
    int32_t top = 0;
    for (int t = 0; t < SECONDS * RACE_SIM_TICKS_PER_SECOND; t++) {
        const int tap = sim.race.tick >= DRIVE_AT && sim.race.tick < DRIVE_AT + 8;
        race_sim_tick(&sim, THROTTLE | (tap ? GEAR_UP_KEY : 0));
        if (car->speed > top) top = car->speed;
    }
    const int ok = top > 0x100000 && car->progress > start + 40;
    printf("%-7s top speed %6.1f m/s, %4d nodes, gear %d, revs %d%s\n", car_model_names[model], top / 65536.0,
           car->progress - start, car->gear, car->rpm, ok ? "" : "  FAIL");
    return ok;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: test_drive SIMDATA_DIR [TRACK]\n");
        return 2;
    }
    const char *track = argc > 2 ? argv[2] : "TR1";
    char path[512];
    snprintf(path, sizeof path, "%s/MISC/%s.TRI", argv[1], track);
    size_t size = 0;
    uint8_t *file = read_file(path, &size);
    Tri tri;
    if (file == NULL || tri_open(&tri, file, size) != TRI_OK) {
        fprintf(stderr, "test_drive: cannot open %s\n", path);
        free(file);
        return 1;
    }
    int ok = 1;
    for (int model = 0; model < 8; model++) ok &= drive(&tri, track, argv[1], model);
    free(file);
    return ok ? 0 : 1;
}
