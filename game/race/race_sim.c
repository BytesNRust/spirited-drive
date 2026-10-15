#include "game/race/race_sim.h"

#include "game/race/race_camera.h"
#include "game/race/race_chase.h"
#include "game/race/race_enter.h"

#include "game/race/ai_drive.h"
#include "game/race/ai_manager.h"
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
#include "game/race/race_grid.h"
#include "game/race/race_models.h"
#include "game/race/race_sound.h"
#include "game/race/race_split_times.h"
#include "game/race/race_frame.h"
#include "game/race/race_tick.h"

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
    COP = 0x8,                   /* flags_174 (00440858) */
    EARLY_LOOP_NODES = 1000,
    STAGE_END = 0xb5,            /* nodes a stage's race stops short of its last (00455a5c) */
    STOPPED = 0x8000,            /* a finished human's speed (+0x70) at most this ends the race */
    END_FINISHED = 1,            /* 00509d6c / 00509d70 */
    END_ALL_OUT = 0x10           /* no human has a finish above 0 */
};

/* RaceSounds.sound: 00443eac, which picks the sound and plays it through
 * 0044404c (the voices are NULL until the audio output exists) */
static void sim_sound(void *user, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f)
{
    race_sound_play(user, a, b, c, d, e, f);
}

static void no_speech(void *user, const char *name)
{
    (void)user, (void)name;
}

static void no_tone(void *user, int32_t a, int32_t b, int32_t c)
{
    (void)user, (void)a, (void)b, (void)c;
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
    static const char LOOPS[] = "5126374";  /* tr5 first: launched as track 3 */
    char a = (char)tolower((unsigned char)stem[0]), b = (char)tolower((unsigned char)stem[1]);
    if (a == 'c' && b == 'y') return 0;
    if (a == 'c' && b == 'l') return 1;
    if (a == 'a' && b == 'l') return 2;
    const char *at = a == 't' && b == 'r' && stem[2] != '\0' ? strchr(LOOPS, stem[2]) : NULL;
    return at == NULL ? -1 : at == LOOPS ? 3 : (int)(at - LOOPS) + 2;
}

int race_sim_track_variant(const char *stem)
{
    const int track = race_sim_track_index(stem);
    return track >= 0 && track < 3 && stem[2] >= '1' && stem[2] <= '3' ? stem[2] - '1' : 0;
}

void race_field_counts(RaceField *field, int track, uint32_t track_flags, int flag_52b312)
{
    field->cops = field->traffic = 0;
    if (field->race_type == 2 || field->race_type == 3) field->racers = RACE_PLAYERS;
    else if (field->race_type == 0) field->racers = 1;
    else {
        field->racers = 2;
        if (!(track_flags & 4) && track < 3) {
            field->cops = flag_52b312 == 0;
            field->traffic = 6;
        }
    }
}

/* RaceSounds.push / crash: 0046a654 on the car, from a blown engine
 * (0046b080, a slot) or a wall hit (004302a4, the car +0x88 names). */
static void sim_push(void *user, int32_t car, int32_t up, int32_t along, int32_t side)
{
    RaceState *race = user;
    Car *pushed = race->cars[race_slot(car)];
    if (pushed != NULL) car_push_spin(pushed, race, up, along, side);
}

static void sim_crash(void *user, uint32_t target, int32_t up, int32_t along, int32_t side)
{
    RaceState *race = user;
    Car *pushed = race_car_by_handle(race, target);
    if (pushed != NULL) car_push_spin(pushed, race, up, along, side);
}

/* 00440858's calls in race_sim: the cars are the sim's slots, each entered with its specs and handle, its PDN
 * (the field's own, else what its load gives) for the body. */
typedef struct {
    RaceSim *sim;
    const RaceField *field;
    const CarPdn *loaded;
} EnterCtx;

static void enter_tables(void *ctx) { (void)ctx; }  /* 0043f994: race_sim_start_field set its words before */
static void enter_load_pick(void *ctx, int32_t first) { race_loading_pick(&((EnterCtx *)ctx)->sim->race, first); }
static void enter_sort(void *ctx) { race_sort_cars(&((EnterCtx *)ctx)->sim->race); }

static Car *enter_alloc(void *ctx, int32_t size)
{
    (void)size;
    RaceSim *sim = ((EnterCtx *)ctx)->sim;
    int32_t i = 0;
    while (i < RACE_CARS - 1 && sim->race.cars[i] != NULL) i++;  /* made in slot order */
    return &sim->slots[i].car;
}

static void enter_car(void *ctx, Car *car)
{
    RaceSim *sim = ((EnterCtx *)ctx)->sim;
    const int i = car->index;
    car_enter(car, &sim->specs[i], CAR_HANDLE * (uint32_t)(i + 1), &sim->race);
}

static void enter_load(void *ctx, Car *car)
{
    EnterCtx *e = ctx;
    e->loaded = e->field->load != NULL ? e->field->load(e->field->user, car, &e->sim->race) : NULL;  /* 00457d40 */
}

static void enter_body(void *ctx, Car *car, int32_t slot)
{
    EnterCtx *e = ctx;
    const CarPdn *pdn = e->field->pdns[slot] != NULL ? e->field->pdns[slot] : e->loaded;
    if (pdn != NULL) car_body_init(car, pdn);
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
    race->track_variant = race_sim_track_variant(stem);
    race->arcade = arcade;
    race->race_type = field->race_type;
    race->racers = field->racers < 1 ? 1 : field->racers > RACE_PLAYERS ? RACE_PLAYERS : field->racers;
    race->cops = field->cops < 0 ? 0 : field->cops;
    race->traffic_setting = field->traffic < 0 ? 0 : field->traffic;
    race->car_count = race->racers + race->cops + race->traffic_setting;  /* 00439c98 */
    if (race->car_count > RACE_CARS) race->car_count = RACE_CARS;     /* port guard: 9 slots */
    race->humans = 1;
    race->phase = 1;
    race->circuit = tri_is_circuit(tri);
    race->segments = (int32_t)tri->ring_segments;  /* 0050420c (0043e4e0): 0x8000 on a stage */
    race->laps_setting = laps;
    race->loop_nodes = EARLY_LOOP_NODES;  /* 0043f994, until 0043fbac sets the real one */
    race_chase_init(race);
    race->chase.kind = 1;
    race->catch_up = CATCH_UP;
    race->unknown_4bc6f0 = 1;  /* 004bc6f0 as the exe starts; 00444394 in 00455a5c's setup sets it again */
    race->siren_count = 3;     /* 004c5fb4 and 004c5fbc as the exe starts */
    race->siren_pitch = 0x40;
    race->crash_sound_level = CRASH_SOUND_QUIET;
    race->sounds = (RaceSounds){ race, sim_sound, no_speech, no_tone, sim_push, sim_crash, no_start_line, no_event, no_event, no_buffet, no_camera, no_jolt, NULL,
                                 NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
    memcpy(race->entries, field->entries, sizeof race->entries);
    /* 004275f8: 0042748c (the grid places), 00428fe0 with 004fb7f4, then
     * 00457870, before 00455a5c. */
    race_grid(race, (int16_t)race->race_type, (int16_t)race->track_variant);
    race_random_seed(race, field->seed);
    race_models(race);
    /* 00455a5c: 004571e8(1), the loading picture (its draws from race_rand).
     * TODO: whether the loads between it and 00440858 draw too is unread. */
    race_loading_pick(race, 1);
    race->controls[0] = (ControlSetup){ 0, 0, 0 };
    sim->count = race->car_count;
    sim->specs[0] = *specs;
    for (int i = 0; i < RACE_CARS; i++) race->by_node[i] = race->by_progress[i] = i;
    EnterCtx ctx = { sim, field, NULL };
    const RaceEnterOps ops = { &ctx, enter_tables, enter_load_pick, enter_alloc, enter_car, enter_load, enter_body,
                               enter_sort };
    race_cars_enter(&ops, race);  /* 00440858 */
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

int race_sim_over(const RaceSim *sim)
{
    return sim->race.end_request != 0;
}

void race_sim_end(RaceSim *sim)
{
    race_results_end(&sim->race);
}

/* race_tick's callees on the port's functions */
static int32_t sim_slot(const RaceState *race, const Car *car)
{
    for (int32_t i = 0; i < RACE_CARS; i++)
        if (race->cars[i] == car) return i;
    return 0;
}

static void sim_replay(void *ctx)
{
    RaceSim *sim = ctx;
    if (sim->replay != NULL) {
        sim->replay(sim->replay_user);
        return;
    }
    sim->race.clock = sim->race.tick % CLOCK_WRAP;  /* 00450020's clock, without a recording */
}

static void sim_controller(void *ctx, Car *car, int32_t kind)
{
    RaceSim *sim = ctx;
    (void)kind;
    run_controller(car, &sim->specs[sim_slot(&sim->race, car)], &sim->race);
}

/* 004054cc / 00405370: reached only on split screens (screen kinds 4 and 5), which race_sim has not */
static void sim_window(void *ctx, int32_t *first, int32_t *size) { (void)ctx, *first = 0, *size = 0; }
static void sim_window_set(void *ctx, int32_t size) { (void)ctx, (void)size; }
static void sim_finish(void *ctx, Car *car) { race_car_finish(car, &((RaceSim *)ctx)->race); }
static void sim_chase_take(void *ctx, Car *car)
{
    RaceState *race = &((RaceSim *)ctx)->race;
    race_chase_follow(race, &race->chase, car, 3);
}

static void sim_engines(void *ctx) { race_sound_start_engine(&((RaceSim *)ctx)->race); }
static int32_t sim_wait_timer;  /* 004bb8d8: reached only in a network game's playback (0052b312 == 1) */
static void sim_service(void *ctx) { (void)ctx, sim_wait_timer++; }
static void sim_physics(void *ctx, Car *car, int32_t kind)
{
    RaceSim *sim = ctx;
    (void)kind;
    run_physics(car, &sim->specs[sim_slot(&sim->race, car)], &sim->race);
}

static void sim_lateral(void *ctx, Car *car) { car_lateral(car, &((RaceSim *)ctx)->race); }
static void sim_sort(void *ctx) { race_sort_cars(&((RaceSim *)ctx)->race); }
static void sim_contacts(void *ctx) { race_cars_contacts(&((RaceSim *)ctx)->race); }
static void sim_splits_pass(void *ctx, int32_t mine, int32_t rival) { race_splits_pass(&((RaceSim *)ctx)->race, mine, rival); }
static void sim_splits_gap(void *ctx) { race_splits_gap(&((RaceSim *)ctx)->race); }
static void sim_camera_pick(void *ctx, Car *car) { race_camera_pick(&((RaceSim *)ctx)->race, car); }
static void sim_chase_tick(void *ctx)
{
    RaceState *race = &((RaceSim *)ctx)->race;
    race_chase_tick(race, &race->chase);
}

static void sim_scrape(void *ctx, Car *car) { race_send_vibration(car, &((RaceSim *)ctx)->race); }
static void sim_force(void *ctx, Car *car) { race_send_force(car, &((RaceSim *)ctx)->race); }
static void sim_follow(void *ctx)
{
    RaceSim *sim = ctx;
    race_sound_follow(&sim->race, &sim->specs[race_slot(sim->race.player)]);
}

static void sim_other_human(void *ctx) { race_sound_other_human(&((RaceSim *)ctx)->race); }
static void sim_held_car(void *ctx) { race_sound_held_car(&((RaceSim *)ctx)->race); }
static void sim_tyres(void *ctx) { race_sound_tyres(&((RaceSim *)ctx)->race); }
static void sim_others(void *ctx) { race_sound_others(&((RaceSim *)ctx)->race, &((RaceSim *)ctx)->police_lights); }

/* 00476f94 (a platform thunk) */
static void sim_none(void *ctx) { (void)ctx; }
static void sim_frame_none(void *ctx, int16_t frame) { (void)ctx, (void)frame; }

static void sim_tick(void *ctx)
{
    static const RaceTickOps ops = { NULL, sim_replay, sim_controller, sim_window, sim_window_set, sim_finish,
                                     sim_chase_take, sim_engines, sim_service, sim_service, sim_physics, sim_lateral,
                                     sim_sort, sim_contacts, sim_splits_pass, sim_splits_gap, sim_camera_pick,
                                     sim_chase_tick, sim_none, sim_scrape, sim_force, sim_held_car, sim_follow, sim_tyres,
                                     sim_other_human, sim_others };
    RaceTickOps o = ops;
    o.ctx = ctx;
    RaceTickWords w = { 0, 0, 0, 0, 0, &sim_wait_timer, { 0, 0 } };
    race_tick(&o, &((RaceSim *)ctx)->race, &w);
}

/* 0045597c: the music, the deferred calls (the in-race menu and pause), the
 * Esc / P keys and 004381f8's commands are --drive's (track_viewer.c) or not
 * yet ported there; the tick and its count as the frame runs them. */
void race_sim_tick_only(RaceSim *sim)
{
    sim_tick(sim);
}

void race_sim_begin(RaceSim *sim)
{
    RaceState *race = &sim->race;
    for (int i = 0; i < 6; i++) race->start_beeps[i] = 1;  /* 00455a62: the setup's words */
    race->tick = race->phase = race->camera_hold = 0;
    race->unknown_513e64 = race->unknown_513e60 = race->unknown_513e6c = 0;
    if (race->race_type == 0 && race->track == 3 && race->cars[0] != NULL) {  /* 00455bd8 */
        race->cars[0]->node = 5;
        race->cars[0]->progress = 5;
        race->cars[0]->z = 0x1e0000;
    }
    race->tick_flags = 0xff;  /* 00455d3a: the dword -1; the ticks run in phase 0 */
    for (int i = 0; i < 3; i++) sim_tick(sim);
    race->unknown_513e64 = 1;
    race->unknown_513e60 = 0;
    race->phase = 1;
    race->replay_mark_countdown = 0;  /* 00455e6d: the first mark at once */
}

void race_sim_tick(RaceSim *sim, uint32_t input)
{
    RaceState *race = &sim->race;
    race->inputs[0] = input;
    race->unknown_513e64 = 1;
    const RaceFrameOps frame = { sim, sim_none, sim_none, sim_frame_none, sim_frame_none, sim_none, sim_tick };
    race_frame(&frame, race, 0, (int16_t)race->tick);
}
