/* port (dep59 step 3): the cars the menus' arrows step through (notes/dep59-step3-card-gallery-plan.md, Task 6): the eight, then in
 * Restomod the added cars lowest number first (car_registry.h), then the Warrior (11) when it is on offer, between the last and car 0 as
 * the original has it between 7 and 0. Stock and Restored never list an added car. */
#ifndef TNFS_CAR_LIST_H
#define TNFS_CAR_LIST_H

#include "game/race/car_registry.h"

#include <stdint.h>

enum { CAR_LIST_MAX = CAR_ORIGINAL_MODELS + CAR_ADDED_MAX + 1 };

typedef struct {
    int16_t car[CAR_LIST_MAX];
    int count;
} CarList;

/* the menus' cars: 0..7, then (`restomod` only) the registered added ones, then 11 when `warrior` */
void car_list_build(CarList *out, int restomod, int warrior);
/* the car `step` (1 on, -1 back) from `car` round the list; a car not in it gives the list's first */
int16_t car_list_step(const CarList *l, int16_t car, int step);

#endif
