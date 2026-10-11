#include "game/race/race_sim.h"

#include "game/race/race_camera.h"
#include "game/race/race_chase.h"

#include "game/race/ai_drive.h"
#include "game/race/ai_setup.h"
#include "game/race/car_collide.h"
#include "game/race/car_crash.h"
#include "game/race/car_held.h"
#include "game/race/car_control.h"
#include "game/race/car_load.h"
#include "game/race/car_physics.h"
#include "game/race/car_road.h"
#include "game/race/race_finish.h"
#include "game/race/race_results.h"
#include "game/race/race_split_times.h"

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
    CRASH_SOUND_QUIET = 0x6666,  /* 004b9094 as loaded, and from 0040d8e8 */
    CAR_HANDLE = 0x10000,        /* the identity the original keeps as the car's address, times the slot + 1 */
    RACER = 0x4,                 /* flags_174 (00440858) */
    EARLY_LOOP_NODES = 1000,
    STAGE_END = 0xb5,            /* nodes a stage's race stops short of its last (00455a5c) */
    STOPPED = 0x8000,            /* a finished human's speed (+0x70) at most this ends the race */
    END_FINISHED = 1,            /* 00509d6c / 00509d70 */
    END_ALL_OUT = 0x10           /* no human has a finish above 0 */
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

static void no_event(void *user)
{
    (void)user;
}

static void no_buffet(void *user, int32_t speed, int32_t kick)
{
    (void)user, (void)speed, (void)kick;
}

static void no_jolt(void *user, int32_t car_a, int32_t car_b, int32_t x, int32_t y, int32_t z, int32_t impulse)
{
    (void)user, (void)car_a, (void)car_b, (void)x, (void)y, (void)z, (void)impulse;
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

void race_sim_start_field(RaceSim *sim, const Tri *tri, const char *stem, const CarSpecs *specs,
                          const RaceField *field, int laps, int arcade)
{
    memset(sim, 0, sizeof *sim);
    RaceState *race = &sim->race;
    race->tri = tri;
    for (size_t i = 0; stem[i] != '\0' && i + 1 < sizeof race->track_name; i++)
        race->track_name[i] = (char)tolower((unsigned char)stem[i]);
    race->track = race_sim_track_index(stem);
    race->arcade = arcade;
    race->race_type = field->race_type;
    race->racers = field->racers < 1 ? 1 : field->racers > RACE_PLAYERS ? RACE_PLAYERS : field->racers;
    race->car_count = race->racers;
    race->humans = 1;
    race->phase = 1;
    race->circuit = tri_is_circuit(tri);
    race->segments = (int32_t)tri->ring_segments;  /* 0050420c (0043e4e0): 0x8000 on a stage */
    race->laps_setting = laps;
    race->loop_nodes = EARLY_LOOP_NODES;  /* 0043f994, until 0043fbac sets the real one */
    race_chase_init(race);
    race->chase.kind = 1;
    race->catch_up = CATCH_UP;
    race->crash_sound_level = CRASH_SOUND_QUIET;
    race->sounds = (RaceSounds){ NULL, no_sound, no_speech, no_tone, no_push, no_crash, no_start_line, no_event, no_event, no_buffet, no_camera, no_jolt, NULL };
    memcpy(race->entries, field->entries, sizeof race->entries);
    race->controls[0] = (ControlSetup){ 0, 0, 0 };
    sim->count = race->car_count;
    sim->specs[0] = *specs;
    for (int i = 0; i < RACE_PLAYERS; i++) race->by_node[i] = race->by_progress[i] = i;
    for (int i = 0; i < sim->count; i++) race->cars[i] = &sim->slots[i].car;
    /* 00440858: each slot entered (004406d0), marked racer or police by its
     * place, then its body from the PDN (004407b8). */
    for (int i = 0; i < sim->count; i++) {
        Car *car = &sim->slots[i].car;
        car->index = i;
        car->player = i;
        car_enter(car, &sim->specs[i], CAR_HANDLE * (uint32_t)(i + 1), race);
        car->flags_51a = (uint8_t)((car->flags_51a | FLAG_ACTIVE) & ~FLAG_RETIRED);
        if (i >= race->humans && i < race->racers) car->flags_174 |= RACER;
        if (field->pdns[i] != NULL) car_body_init(car, field->pdns[i]);
    }
    for (int i = 0; i < sim->count; i++) sim->slots[i].car.lateral = 0;
    race_sort_cars(race);
    race->manager_countdown = 1;  /* 0040fec8 */
    /* 00455a5c runs 004702d4 after the cars are entered (00440858); the
     * row it passes is what 00475c14(0) leaves in al, unknown, and only
     * read for race types above 3 or models above 7. */
    ai_race_setup(race, 0);
    /* 00455a5c -> 0043fbac: a circuit's lap is its ring of nodes, run the
     * laps chosen; a stage is run once, to 0xb5 nodes before its end. */
    if (race->circuit) race_laps_start(race, race->segments * 4, laps);
    else race_laps_start(race, (int32_t)tri->segment_count * 4 - STAGE_END, 1);
}

/* The progress the split times measure the player against (00440ef0):
 * the other car when two drive or one opponent races, else the leader, or
 * the second when the player leads; none without opponents. */
static int32_t rival_progress(const RaceState *race)
{
    const int32_t opponents = race->racers - race->humans;
    if (race->humans >= 2 || opponents == 1) return race->cars[race->player == 0 ? 1 : 0]->progress;
    if (opponents < 2) return 0;
    const int32_t leader = race->by_progress[(race->car_count - 1) & (RACE_PLAYERS - 1)];
    const int32_t rival = leader == race->player ? race->by_progress[(race->car_count - 2) & (RACE_PLAYERS - 1)] : leader;
    return race->cars[rival & (RACE_PLAYERS - 1)]->progress;
}

void race_sim_start(RaceSim *sim, const Tri *tri, const char *stem, const CarSpecs *specs, const RaceEntry *entry,
                    int laps, int arcade)
{
    RaceField field;
    memset(&field, 0, sizeof field);
    field.racers = 1;
    field.entries[0] = *entry;
    race_sim_start_field(sim, tri, stem, specs, &field, laps, arcade);
}

static void run_controller(Car *car, const CarSpecs *specs, RaceState *race)
{
    /* Controller 2 (0046d5c4) is a bare `ret`: the computer cars are driven
     * by their physics model and the manager. */
    if (car->controller == CONTROLLER_PLAYER) car_player_control(car, specs, race);
}

/* The 0043f994 table: 1 held 00470e70, 2 driving 004286a0, 3 computer
 * 0046d79c, 4 crash 0040e050, 5 spun 0046fe34, 6 parked 00470518; then the
 * lateral distance (00440a68). */
static void run_physics(Car *car, CarSpecs *specs, RaceState *race)
{
    switch (car->physics) {
    case 1: car_held_tick(car, race); break;
    case PHYSICS_DRIVING: car_driving_tick(car, specs, race); break;
    case 3: ai_model_tick(car, race); break;
    case 4: car_crash_tick(car, race); break;
    case 5: ai_spun_tick(car, race); break;
    case 6: ai_parked_tick(car, race); break;
    default: break;
    }
    car_lateral(car, race);
}

/* 00440ef0 after the finishes: once every human (finish_humans) has a
 * finish and has slowed to STOPPED, the end request: 1, or 0x10 when none
 * of them finished above 0. Not ported: the split screen's switch to the
 * other player's car (00404530, the camera) and the network wait
 * (0052b312 == 1). */
static void check_end(RaceState *race)
{
    int32_t humans = race->finish_humans;
    if (humans > RACE_PLAYERS) humans = RACE_PLAYERS;  /* port guard */
    for (int32_t i = 0; i < humans; i++)
        if (race->players[i].finish == 0 || race->cars[i]->speed > STOPPED) return;
    int all_out = 1;
    for (int32_t i = 0; i < humans; i++)
        if (race->players[i].finish > 0) all_out = 0;
    race->end_request = race->end_state = all_out ? END_ALL_OUT : END_FINISHED;
}

int race_sim_over(const RaceSim *sim)
{
    return sim->race.end_request != 0;
}

void race_sim_end(RaceSim *sim)
{
    race_results_end(&sim->race);
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
        for (int i = 0; i < race->car_count; i++) {  /* road order too */
            const int slot = race->by_node[i] & (RACE_PLAYERS - 1);
            Car *car = race->cars[slot];
            if ((car->flags_51a & FLAG_ACTIVE) && car->controller != 0) run_controller(car, &sim->specs[slot], race);
        }
    if (race->tick_flags & TICK_PHYSICS) {
        /* 00440ef0: each racing car's finish first, by slot. */
        if (race->phase == 1)
            for (int i = 0; i < race->finish_cars && i < sim->count; i++)
                if (race->cars[i]->flags_51a & FLAG_ACTIVE) race_car_finish(race->cars[i], race);
        if (race->phase == 1) check_end(race);
        /* Then in road order, the rearmost first. */
        for (int i = 0; i < race->car_count; i++) {
            const int slot = race->by_node[i] & (RACE_PLAYERS - 1);
            Car *car = race->cars[slot];
            if ((car->flags_51a & FLAG_ACTIVE) && car->physics != 0) run_physics(car, &sim->specs[slot], race);
        }
        race_sort_cars(race);
        race_cars_contacts(race);
        race_sort_cars(race);
        race_splits_pass(race, race->cars[race->player & (RACE_PLAYERS - 1)]->progress, rival_progress(race));
        race_splits_gap(race);
        race_camera_pick(race, race->cars[race->viewed & (RACE_PLAYERS - 1)]);
        race_chase_tick(race, &race->chase);
        for (int i = 0; i < sim->count; i++)
            if ((sim->slots[i].car.flags_51a & FLAG_ACTIVE) && sim->slots[i].car.controller != 0)
                car_keep_draw_state(&sim->slots[i].car);
    }
    race->tick += 2;
}
