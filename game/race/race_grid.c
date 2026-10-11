#include "game/race/race_grid.h"

#include <stdint.h>

enum {
    GRID_CARS = 8,
    STANDINGS = 0x570,          /* 0052b2b4 in the game state: a short a car */
    POINTS_FIRST = 0x592,       /* 0052b2d6..0052b2e6: 9 shorts */
    POINTS_WORDS = 9
};

/* 0042748c's fixed places by slot. */
static const int32_t FIXED[GRID_CARS] = { 7, 6, 2, 3, 4, 5, 1, 0 };

static int16_t state_short(const RaceState *race, int at)
{
    return (int16_t)(race->game_state[at] | race->game_state[at + 1] << 8);
}

static int no_points(const RaceState *race)
{
    for (int i = 0; i < POINTS_WORDS; i++)
        if (state_short(race, POINTS_FIRST + 2 * i) != 0) return 0;
    return 1;
}

/* Slots by standing, the most first (a bubble sort, swapping when the
 * earlier has fewer); each slot's place its rank. */
static void by_standings(RaceState *race)
{
    int32_t order[GRID_CARS];
    for (int i = 0; i < GRID_CARS; i++) order[i] = i;
    int swapped;
    do {
        swapped = 0;
        for (int i = 0; i < GRID_CARS - 1; i++)
            if (state_short(race, STANDINGS + 2 * order[i]) < state_short(race, STANDINGS + 2 * order[i + 1])) {
                const int32_t t = order[i];
                order[i] = order[i + 1];
                order[i + 1] = t;
                swapped = 1;
            }
    } while (swapped);
    for (int k = 0; k < GRID_CARS; k++) race->grid[order[k]] = k;
}

void race_grid(RaceState *race, int16_t race_type, int16_t variant)
{
    if (race_type != 3 && race_type != 2) {
        race->grid[0] = 7;
        race->grid[1] = 6;
        return;
    }
    if (variant != 0) {
        for (int k = 0; k < GRID_CARS; k++) {
            const int16_t slot = race->results_order_short[k];
            if (slot >= 0 && slot < GRID_CARS) race->grid[slot] = k;  /* port guard: the original writes anywhere */
        }
        return;
    }
    if (race_type == 2 || no_points(race)) {
        for (int i = 0; i < GRID_CARS; i++) race->grid[i] = FIXED[i];
        return;
    }
    by_standings(race);
}
