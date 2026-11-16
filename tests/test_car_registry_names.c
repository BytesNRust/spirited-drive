/* dep59 step 1: the results and records name a car out of a table of the twelve (results_screen.h's car_names), and an added car (16 on)
 * is past it: car_registry_short_name gives its own short name, "" for a number that is no car, and the table's for the twelve. The menus'
 * numbering and the race's (race_short_name_index, race_head_to_head_model) leave an added car's number as it is. */
#include "game/frontend/results_screen.h"
#include "game/race/car_registry.h"
#include "game/race/race_models.h"
#include "game/race/race_results.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static const char *const NAMES[RESULTS_CARS] = { "RX-7", "NSX", "SUPRA", "911", "ZR-1", "VIPER", "512TR", "DIABLO", "PACK", "CLOCK", "CLASS", "WARRIOR" };

static void register_f40(void)
{
    CarPiece p;
    memset(&p, 0, sizeof p);
    snprintf(p.id, sizeof p.id, "f40");
    snprintf(p.stem, sizeof p.stem, "F40");
    snprintf(p.short_name, sizeof p.short_name, "F40S");
    p.sound_model = 4, p.ai = (CarAiTuning){ CAR_AI_PACE_MIN, CAR_AI_COAST_DEFAULT }, p.dash_model = -1;
    char why[160];
    car_registry_clear();
    CHECK_EQ_INT(car_registry_add(&p, why, sizeof why), 16);
}

static void test_short_names(void)
{
    register_f40();
    for (int car = 0; car < RESULTS_CARS; car++) CHECK(car_registry_short_name(car, NAMES, RESULTS_CARS) == NAMES[car]);
    CHECK(strcmp(car_registry_short_name(16, NAMES, RESULTS_CARS), "F40S") == 0);
    CHECK(strcmp(car_registry_short_name(12, NAMES, RESULTS_CARS), "") == 0);
    CHECK(strcmp(car_registry_short_name(17, NAMES, RESULTS_CARS), "") == 0);
    CHECK(strcmp(car_registry_short_name(-1, NAMES, RESULTS_CARS), "") == 0);
    CHECK(strcmp(car_registry_short_name(0x7fffffff, NAMES, RESULTS_CARS), "") == 0);
    ResultsInputs in;
    memset(&in, 0, sizeof in);
    in.car_names = NAMES;
    CHECK(results_car_name(&in, 5) == NAMES[5]);   /* the screens' reader */
    CHECK(strcmp(results_car_name(&in, 16), "F40S") == 0);
    CHECK(strcmp(results_car_name(&in, 40), "") == 0);
}

static void test_numberings(void)
{
    static const int SHORT[16] = { 2, 7, 3, 4, 6, 5, 1, 0, 8, 9, 10, 11, 12, 13, 14, 15 };
    static const int HEAD[16] = { 7, 6, 0, 2, 3, 5, 4, 1, 8, 9, 10, 11, 12, 13, 14, 15 };
    car_registry_clear();
    CHECK_EQ_INT(race_short_name_index(16), 0);   /* no car 16: as before */
    CHECK_EQ_INT(race_head_to_head_model(16), 0);
    register_f40();
    for (int i = 0; i < 16; i++) {
        CHECK_EQ_INT(race_short_name_index((int16_t)i), SHORT[i]);
        CHECK_EQ_INT(race_head_to_head_model((int16_t)i), HEAD[i]);
    }
    CHECK_EQ_INT(race_short_name_index(16), 16);
    CHECK_EQ_INT(race_head_to_head_model(16), 16);
    CHECK_EQ_INT(race_short_name_index(17), 0);
    CHECK_EQ_INT(race_head_to_head_model(17), 0);
}

int main(void)
{
    test_short_names();
    test_numberings();
    return TEST_REPORT();
}
