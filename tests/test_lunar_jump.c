/* The moon's gravity (track_gravity.h) in a race: the rally on Rusty Springs driven at full throttle with the exe's pull and with
 * the moon's; the flights last longer on the moon. Needs the game's data, skips without it.
 *   test_lunar_jump SIMDATA_DIR */
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

static int longest_flight(const Tri *tri, const char *simdata, int moon, int *flights)
{
    char path[512];
    snprintf(path, sizeof path, "%s/CARSPECS/%s.PBS", simdata, car_model_names[0]);
    size_t size = 0;
    uint8_t *file = read_file(path, &size);
    CarSpecs specs;
    const int read = file != NULL && car_specs_read(&specs, file, size);
    free(file);
    if (!read) return -1;
    RaceEntry entry;
    memset(&entry, 0, sizeof entry);
    strcpy(entry.name, "Test");
    entry.automatic = 1;
    RaceField field;
    memset(&field, 0, sizeof field);
    field.racers = 1;
    field.entries[0] = entry;
    field.port_lunar_gravity = moon;
    race_sim_start_field(&sim, tri, "TR5", &specs, &field, 2, 1);
    const Car *car = sim.race.cars[0];
    int longest = 0;
    *flights = 0;
    for (int t = 0; t < SECONDS * RACE_SIM_TICKS_PER_SECOND; t++) {
        const int tap = sim.race.tick >= DRIVE_AT && sim.race.tick < DRIVE_AT + 8;
        race_sim_tick(&sim, THROTTLE | (tap ? GEAR_UP_KEY : 0));
        if (car->airborne_ticks > longest) longest = car->airborne_ticks;
        if (car->airborne_ticks == 1) (*flights)++;
    }
    return longest;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: test_lunar_jump SIMDATA_DIR\n");
        return 2;
    }
    char path[512];
    snprintf(path, sizeof path, "%s/MISC/TR5.TRI", argv[1]);
    size_t size = 0;
    uint8_t *file = read_file(path, &size);
    Tri tri;
    if (file == NULL || tri_open(&tri, file, size) != TRI_OK) {
        printf("skip (no %s)\n", path);
        free(file);
        return 0;
    }
    int exe_flights = 0, moon_flights = 0;
    const int exe = longest_flight(&tri, argv[1], 0, &exe_flights), moon = longest_flight(&tri, argv[1], 1, &moon_flights);
    free(file);
    if (exe < 0) {
        printf("skip (no car specs)\n");
        return 0;
    }
    printf("longest flight: %d ticks with the exe's pull (%d flights), %d on the moon (%d)\n", exe, exe_flights, moon, moon_flights);
    return moon > exe && exe > 0 ? 0 : 1;
}
