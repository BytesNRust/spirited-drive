/* port (dep59): the cars the menus' arrows step through. The standard edition's (notes/dep59-step4-editions-plan.md, T6): the eight,
 * then the Warrior (11) when it is on offer, between 7 and 0 as the original has it. An edition's: its cars= in its order, a stock car
 * by its number (formats/stock_content.h), an added one by its registered number (car_registry.h), one that is not there left out; the
 * Warrior when it lists it. */
#ifndef TNFS_CAR_LIST_H
#define TNFS_CAR_LIST_H

#include "formats/edition_ini.h"
#include "game/race/car_registry.h"

#include <stdint.h>

enum { CAR_LIST_MAX = CAR_ORIGINAL_MODELS + CAR_ADDED_MAX + 1 };

typedef struct {
    int16_t car[CAR_LIST_MAX];
    int count;
} CarList;

/* the menus' cars: `e` NULL the standard edition's (0..7, then 11 when `warrior`); else the edition's (`warrior` not read) */
void car_list_build(CarList *out, const Edition *e, int warrior);
/* 1 when `car` is in the list */
int car_list_has(const CarList *l, int16_t car);
/* the car `step` (1 on, -1 back) from `car` round the list; a car not in it gives the list's first */
int16_t car_list_step(const CarList *l, int16_t car, int step);

#endif
