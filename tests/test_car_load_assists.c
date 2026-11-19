/* port (dep78): the physics sees the capability the car's piece gives (its mod.ini's abs= / traction=, else its PBS): car_load sets the specs'
 * words at 0x334 / 0x338 to it for an added car, and the gate (the player's choice AND the capability) is as before for an original. */
#include "game/race/car_load.h"
#include "game/race/car_registry.h"
#include "game/race/car_specs.h"
#include "test.h"

#include <string.h>

static RaceState race;
static Car car;
static CarSpecs specs;

/* one car_load of car number `model` with the player's choices `abs`, `traction` and the PBS words `w334`, `w338` */
static void load(int model, int abs, int traction, int32_t w334, int32_t w338)
{
    memset(&race, 0, sizeof race);
    memset(&car, 0, sizeof car);
    memset(&specs, 0, sizeof specs);
    specs.unknown_334 = w334, specs.unknown_338 = w338;
    race.specs_loaded = 1;
    race.cars[0] = &car;
    race.entries[0].car = model, race.entries[0].anti_lock = abs, race.entries[0].traction = traction;
    car_load(&car, &specs, &race);
}

int main(void)
{
    car_registry_clear();
    const CarAiTuning ai = { CAR_AI_PACE_MIN, CAR_AI_COAST_DEFAULT };
    const int yes_no = car_registry_add(&(CarPiece){ .id = "a", .stem = "A1", .sound_model = 5, .ai = ai, .dash_model = -1, .abs_on = 1 }, NULL, 0);
    const int no_yes = car_registry_add(&(CarPiece){ .id = "b", .stem = "B1", .sound_model = 5, .ai = ai, .dash_model = -1, .traction_on = 1 }, NULL, 0);
    const int none = car_registry_add(&(CarPiece){ .id = "c", .stem = "C1", .sound_model = 5, .ai = ai, .dash_model = -1 }, NULL, 0);
    CHECK(yes_no >= CAR_ADDED_FIRST && no_yes >= CAR_ADDED_FIRST && none >= CAR_ADDED_FIRST);

    /* a piece that has an assist works with the PBS words at 0: the mod.ini said yes */
    load(yes_no, 1, 1, 0, 0);
    CHECK_EQ_INT(car.anti_lock, 1);
    CHECK_EQ_INT(car.traction_control, 0);
    CHECK_EQ_INT(specs.unknown_334, CAR_SPECS_ASSIST_ON);
    CHECK_EQ_INT(specs.unknown_338, 0);
    load(no_yes, 1, 1, 0, 0);
    CHECK_EQ_INT(car.anti_lock, 0);
    CHECK_EQ_INT(car.traction_control, 1);
    /* and one that has not stays without it with the PBS words positive: the mod.ini said no */
    load(none, 1, 1, 1, 1);
    CHECK_EQ_INT(car.anti_lock, 0);
    CHECK_EQ_INT(car.traction_control, 0);
    /* the player's choice still gates it */
    load(yes_no, 0, 0, 0, 0);
    CHECK_EQ_INT(car.anti_lock, 0);
    load(no_yes, 0, 0, 0, 0);
    CHECK_EQ_INT(car.traction_control, 0);

    /* the CD's cars: the PBS words and the player's choice, as before */
    load(0, 1, 1, 1, 1);
    CHECK_EQ_INT(car.anti_lock, 1);
    CHECK_EQ_INT(car.traction_control, 1);
    load(0, 1, 1, 0, 1);
    CHECK_EQ_INT(car.anti_lock, 0);
    CHECK_EQ_INT(car.traction_control, 1);
    load(0, 0, 1, 1, 1);
    CHECK_EQ_INT(car.anti_lock, 0);
    CHECK_EQ_INT(specs.unknown_334, 1);   /* an original's words are not touched */
    load(7, 1, 1, 0, 0);
    CHECK_EQ_INT(specs.unknown_334, 0);
    car_registry_clear();
    return TEST_REPORT();
}
