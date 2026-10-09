#include "game/race/race_sim.h"

#include "game/race/car_control.h"
#include "game/race/car_load.h"
#include "game/race/car_physics.h"
#include "game/race/car_road.h"

#include <ctype.h>
#include <string.h>

enum {
    CONTROLLER_PLAYER = 1,
    PHYSICS_DRIVING = 2,
    FLAG_ACTIVE = 4,             /* flags_51a */
    FLAG_RETIRED = 8,
    TICK_PHYSICS = 1,            /* 004fcdb0, set by 0045597c */
    TICK_CONTROLS = 4,
    TICK_AIDS = 0x10,
    REST_TICKS = 0x78,           /* the inputs stay at rest this long */
    CLOCK_WRAP = 0x7080,
    CATCH_UP = 4,                /* 0042702c */
    CAR_HANDLE = 0x10000         /* the identity the original keeps as the car's address */
};

static void no_sound(void *user, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f)
{
    (void)user, (void)a, (void)b, (void)c, (void)d, (void)e, (void)f;
}

static void no_speech(void *user, const char *name)
{
    (void)user, (void)name;
}

static void no_tone(void *user, int32_t a, int32_t b, int32_t c)
{
    (void)user, (void)a, (void)b, (void)c;
}

static void no_push(void *user, int32_t car, int32_t x, int32_t y, int32_t z)
{
    (void)user, (void)car, (void)x, (void)y, (void)z;
}

static void no_crash(void *user, uint32_t target, int32_t x, int32_t y, int32_t z)
{
    (void)user, (void)target, (void)x, (void)y, (void)z;
}

static void no_start_line(void *user, int32_t player)
{
    (void)user, (void)player;
}

static void no_camera(void *user, int32_t slot, int32_t mode)
{
    (void)user, (void)slot, (void)mode;
}

int race_sim_track_index(const char *stem)
{
    char a = (char)tolower((unsigned char)stem[0]), b = (char)tolower((unsigned char)stem[1]);
    if (a == 'c' && b == 'y') return 0;
    if (a == 'c' && b == 'l') return 1;
    if (a == 'a' && b == 'l') return 2;
    if (a == 't' && b == 'r' && stem[2] >= '1' && stem[2] <= '7') return stem[2] - '1' + 3;
    return -1;
}

void race_sim_start(RaceSim *sim, const Tri *tri, const char *stem, const CarSpecs *specs, const RaceEntry *entry,
                    int laps, int arcade)
{
    memset(sim, 0, sizeof *sim);
    RaceState *race = &sim->race;
    race->tri = tri;
    for (size_t i = 0; stem[i] != '\0' && i + 1 < sizeof race->track_name; i++)
        race->track_name[i] = (char)tolower((unsigned char)stem[i]);
    race->track = race_sim_track_index(stem);
    race->arcade = arcade;
    race->car_count = 1;
    race->humans = 1;
    race->phase = 1;
    race->circuit = tri_is_circuit(tri);
    race->segments = tri->segment_count;
    /* TODO: 00509d58 is 1000 from 0043f994; the track setup that sets the
     * real lap length is not ported yet. */
    race->loop_nodes = (int32_t)tri->node_count;
    race->laps = laps;
    race->views = 1;
    race->catch_up = CATCH_UP;
    race->sounds = (RaceSounds){ NULL, no_sound, no_speech, no_tone, no_push, no_crash, no_start_line, no_camera };
    race->entries[0] = *entry;
    race->controls[0] = (ControlSetup){ 0, 0, 0 };
    sim->count = 1;
    sim->specs[0] = *specs;
    Car *car = &sim->cars[0];
    race->cars[0] = car;
    for (int i = 0; i < RACE_PLAYERS; i++) race->by_node[i] = race->by_progress[i] = i;
    car->index = 0;
    car->player = 0;
    car_enter(car, &sim->specs[0], CAR_HANDLE, race);
    car->flags_51a = (uint8_t)((car->flags_51a | FLAG_ACTIVE) & ~FLAG_RETIRED);
}

static void run_controller(Car *car, const CarSpecs *specs, RaceState *race)
{
    if (car->controller == CONTROLLER_PLAYER) car_player_control(car, specs, race);
    /* TODO(0046d5c4): the computer cars' controller (2). */
}

static void run_physics(Car *car, const CarSpecs *specs, RaceState *race)
{
    if (car->physics == PHYSICS_DRIVING) car_driving_tick(car, specs, race);
    /* TODO(00470e70, 0046d79c, 0040e050, 0046fe34, 00470518): the other models. */
    car_lateral(car, race);
}

void race_sim_tick(RaceSim *sim, uint32_t input)
{
    RaceState *race = &sim->race;
    race->tick_flags = (uint8_t)(((race->tick & 1) == 0 ? TICK_PHYSICS : 0) |
                                 ((race->tick & 3) == 0 ? TICK_CONTROLS : 0) | ((race->tick & 7) == 0 ? TICK_AIDS : 0));
    race->clock = race->tick % CLOCK_WRAP;  /* 00450020 */
    race->inputs[0] = input;
    if (race->tick < REST_TICKS)
        for (int i = 0; i < race->humans; i++) race->inputs[i] = RACE_SIM_INPUT_REST;
    if (race->tick_flags & TICK_CONTROLS)
        for (int i = 0; i < sim->count; i++)
            if ((sim->cars[i].flags_51a & FLAG_ACTIVE) && sim->cars[i].controller != 0)
                run_controller(&sim->cars[i], &sim->specs[i], race);
    if (race->tick_flags & TICK_PHYSICS) {
        for (int i = 0; i < sim->count; i++)
            if ((sim->cars[i].flags_51a & FLAG_ACTIVE) && sim->cars[i].physics != 0)
                run_physics(&sim->cars[i], &sim->specs[i], race);
        race_sort_cars(race);
        /* TODO(0040d8e8): the collisions between cars. */
        for (int i = 0; i < sim->count; i++)
            if ((sim->cars[i].flags_51a & FLAG_ACTIVE) && sim->cars[i].controller != 0)
                car_keep_draw_state(&sim->cars[i]);
    }
    race->tick += 2;
}
