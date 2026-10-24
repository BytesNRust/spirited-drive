/* port: a single race races the rival the menus picked (single_rival.h), through 00457870's grid: every computer car
 * the picked model, or the eight different cars for the whole field. */
#include "race/race_models.h"
#include "race/single_rival.h"
#include "test.h"

#include <stdlib.h>

enum { HUMAN_SET = 200, RACERS = 8, SINGLE_RACE = 2 };

static void test_the_pick_is_kept_from_the_menus(void)
{
    for (int pick = 0; pick <= 8; pick++) CHECK_EQ_INT(single_rival_car(1, pick), pick);
    CHECK_EQ_INT(single_rival_car(1, -1), SINGLE_RIVAL_FIELD);   /* nothing sensible: the field */
    CHECK_EQ_INT(single_rival_car(1, 11), SINGLE_RIVAL_FIELD);
}

static void test_without_the_menus_it_is_the_whole_field(void)
{
    for (int pick = 0; pick <= 8; pick++) CHECK_EQ_INT(single_rival_car(0, pick), SINGLE_RIVAL_FIELD);
}

static RaceState *grid(int player_car, int rival)
{
    RaceState *race = calloc(1, sizeof *race);
    race->race_type = SINGLE_RACE;
    race->humans = 1;
    race->racers = RACERS;
    race->player = 0;
    race->entries[0].car = player_car;
    race->entries[1].car = rival;
    race_models(race);
    return race;
}

static void test_a_picked_car_fills_the_grid(void)
{
    for (int car = 0; car < 8; car++) {
        RaceState *race = grid(car == 4 ? 5 : 4, single_rival_car(1, car));
        CHECK_EQ_INT(race->models[0], (car == 4 ? 5 : 4) + HUMAN_SET);
        for (int i = 1; i < RACERS; i++) CHECK_EQ_INT(race->models[i], car + HUMAN_SET);
        free(race);
    }
}

static void test_the_whole_field_is_the_mixed_pack(void)
{
    RaceState *race = grid(4, single_rival_car(1, 8));
    int seen[16] = { 0 }, distinct = 0;
    for (int i = 1; i < RACERS; i++) {
        const int m = race->models[i] - HUMAN_SET;
        CHECK(m >= 0 && m < 8);
        if (m >= 0 && m < 16 && !seen[m]) seen[m] = 1, distinct++;
    }
    CHECK_EQ_INT(distinct, 7);
    free(race);
}

int main(void)
{
    test_the_pick_is_kept_from_the_menus();
    test_without_the_menus_it_is_the_whole_field();
    test_a_picked_car_fills_the_grid();
    test_the_whole_field_is_the_mixed_pack();
    return TEST_REPORT();
}
