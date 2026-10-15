/* The cars entering the race (race_enter.h), as 00440858 runs it. */
#include "game/race/race_enter.h"

enum { LOAD_PICK_SLOT = 2, FLAG_ACTIVE = 0x4, FLAG_RETIRED = 0x8, RACER = 0x4, COP = 0x8 };

/* 00440875..00440916: slot i's car. */
static void enter_slot(const RaceEnterOps *ops, RaceState *race, int32_t i)
{
    if (i == LOAD_PICK_SLOT) ops->load_pick(ops->ctx, 0);
    Car *car = ops->alloc(ops->ctx, CAR_SIZE);
    car->index = i;
    car->player = i;
    race->cars[i] = car;
    race->by_node[i] = i;
    race->by_progress[i] = i;
    ops->enter(ops->ctx, car);
    car->flags_51a = (uint8_t)((car->flags_51a | FLAG_ACTIVE) & ~FLAG_RETIRED);
    if (i >= race->humans && i < race->racers) car->flags_174 |= RACER;
    if (i >= race->racers && i < race->racers + race->cops) car->flags_174 |= COP;
    ops->load(ops->ctx, car);
    ops->body(ops->ctx, car, i);
}

void race_cars_enter(const RaceEnterOps *ops, RaceState *race)
{
    ops->tables(ops->ctx);
    for (int32_t i = 0; i < race->car_count && i < RACE_CARS; i++) enter_slot(ops, race, i);  /* port guard: 9 slots */
    for (int32_t i = 0; i < race->car_count && i < RACE_CARS; i++) race->cars[i]->lateral = 0;
    ops->sort(ops->ctx);
}

void race_cars_leave(const RaceLeaveOps *ops, Car *const *cars, int32_t humans, int32_t car_count,
                     int32_t specs_loaded)
{
    for (int32_t i = 0; i < humans && i < RACE_CARS; i++) {
        ops->free(ops->ctx, cars[i]->specs);
        if (specs_loaded != 0) {
            ops->free(ops->ctx, cars[i]->unknown_471);
            ops->free(ops->ctx, cars[i]->unknown_475);
        }
    }
    for (int32_t i = 0; i < car_count && i < RACE_CARS; i++) {
        Car *car = cars[i];
        ops->release(ops->ctx, car->body_object);
        ops->release(ops->ctx, car->far_object);
        ops->free(ops->ctx, car->model_file);
        ops->free_car(ops->ctx, car);
    }
}
