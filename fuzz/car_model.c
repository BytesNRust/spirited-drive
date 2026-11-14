/* A car's model file (.cfm, the CD's or a mod's) as the race loads it: the meshes into objects, the palettes, the tyres, the signals
 * and lights the player's car needs */
#include "fuzz/fuzz.h"
#include "game/race/car_model.h"
#include "game/race/race_state.h"

#include <stdlib.h>
#include <string.h>

enum { HUMAN_MODEL = 200 };   /* car_model.c's HUMAN_SET: the player's set, so the tyres and the signals are read too */

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    static RaceState race;
    static Car car;
    static RacePalette pal;
    static CarLoad m;
    memset(&race, 0, sizeof race), memset(&car, 0, sizeof car), memset(&pal, 0, sizeof pal), memset(&m, 0, sizeof m);
    race.player = 0, race.models[0] = HUMAN_MODEL, race.cars[0] = &car;
    uint8_t *file = malloc(size ? size : 1);   /* its palettes are changed in place */
    if (file == NULL) return 0;
    if (size) memcpy(file, data, size);
    (void)car_load_setup(&car, &race, &pal, NULL, &m, file, size);
    car_load_free(&m);
    free(file);
    return 0;
}
