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
#include <stddef.h>
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
    NO_MERCY = 4,                /* track_flags bit 2: 0042702c's 004fbac0 then 0 */
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

/* 0044435c / 00444394: the followed car's engine sounds stopped as it crashes and started again once it is back
 * (the latter lets the controls' buffeting go) */
static void sim_stop_engine(void *user) { race_sound_stop_engine(user); }
static void sim_start_engine(void *user) { race_sound_start_engine(user); }

static void no_buffet(void *user, int32_t speed, int32_t kick)
{
    (void)user, (void)speed, (void)kick;
}

static void no_jolt(void *user, int32_t car_a, int32_t car_b, int32_t x, int32_t y, int32_t z, int32_t impulse)
{
    (void)user, (void)car_a, (void)car_b, (void)x, (void)y, (void)z, (void)impulse;
}

/* 00404530(004f801c, 00509e1c[slot], mode): the replay's cuts and a police car placed before the start take the view */
static void sim_camera(void *user, int32_t slot, int32_t mode)
{
    RaceState *race = user;
    if (slot >= 0 && slot < RACE_CARS) race_chase_follow(race, &race->chase, race->cars[slot], mode);
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
/* 004571e8: the pick, then the picture of the followed player's car */
static void loading_slide(RaceState *race, const RaceField *field, int clear)
{
    const int32_t picture = race_loading_pick(race, clear);
    if (picture >= 0 && field->slide != NULL)
        field->slide(field->user, picture, race->entries[race->player & (RACE_PLAYERS - 1)].car);
}

static void enter_load_pick(void *ctx, int32_t first)
{
    EnterCtx *e = ctx;
    loading_slide(&e->sim->race, e->field, first);
}
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

/* What the original keeps from the race before: its race words are globals no race clears, each start setting again
 * only its own (00455a5c and 004275f8 with what they call write none of these). Among them the last results' order
 * (004fa030, 0042748c's grid on a later segment), the opponents' drivers (004fba70, kept past segment 0 by 004702d4),
 * the player records (0051cc54: 0045aab4 clears their bests and keeps a time trial's rival splits at +0x4c), 0051cc50
 * (0045aab4 clears it on segment 0), the surfaces (004fa9b0: on AL3 race_surfaces_init leaves the third one's off_road
 * as AL2 set it), and the words only the race's ticks write. The ones the --drive's glue sets after the start
 * (replay, playback, effects, records, start_input, unknown_4c5cd4) are left to it. */
#define KEEP(f) { offsetof(RaceState, f), sizeof(((RaceState *)0)->f) }
static const struct { size_t at, size; } KEPT[] = {
    KEEP(players), KEEP(results_order_short), KEEP(driver_letters), KEEP(splits.unknown_cc50), KEEP(surfaces),
    KEEP(unknown_4c5398), KEEP(clock), KEEP(last_node), KEEP(stop_notice_tick), KEEP(unknown_4c538c),
    KEEP(crash_sound_level), KEEP(crash_sound_ticks), KEEP(car_impulse), KEEP(trail_seed), KEEP(launch),
    KEEP(launch_full_until), KEEP(launch_end), KEEP(launch_ready), KEEP(wheelspin), KEEP(tyre_force_sum),
    KEEP(crash_spin), KEEP(drawn), KEEP(drawn_count), KEEP(car_places), KEEP(wheel_tick), KEEP(wheel_counter),
    KEEP(wheel_toggle), KEEP(replay_cut_count), KEEP(unknown_4c5370), KEEP(arcade_turn), KEEP(arcade_grip),
    KEEP(arcade_turn_limit), KEEP(strip_brake_80_tick), KEEP(strip_brake_60_tick), KEEP(strip_best_60),
    KEEP(strip_best_100), KEEP(strip_best_quarter), KEEP(strip_start_tick), KEEP(strip_start_progress),
    KEEP(engine_sound), KEEP(sound_pitch), KEEP(camera_tilt), KEEP(default_profile_odds), KEEP(ai.curve_pace),
    KEEP(ai.bend), KEEP(ai.gap_step), KEEP(ai.unknown_364), KEEP(ai.last_lane_width),
    KEEP(unknown_4c539c), KEEP(chase_auto), KEEP(sound_silent)
};
#undef KEEP

enum {
    STRIP_UNSET_AT_START = 99999,   /* 004bae84, 004bae88, 004bae8c */
    NOTICE_TICK_AT_START = -1000,   /* 004b92e4 */
    REPLAY_TICKS_AT_START = 4,      /* 004c5398 */
    REPLAY_PAUSE_AT_START = 2,      /* 004c539c */
    CAMERA_TILT_AT_START = 0x10000, /* 004b91cc, never written */
    PROFILE_ODDS_AT_START = 40,     /* 004c5d20, never written */
    ENGINE_LEVEL_AT_START = 0x7f,   /* 004bcb6c */
    ENGINE_CHANNEL = 7,             /* 004bcb70, never written: the engine's second voice */
    HELD_PITCH_AT_START = 0x40,     /* 004bc6f4..004bc700 */
    HELD_PITCHES = 4
};

/* The kept words as the exe starts with them (its data, DGROUP; the rest of them start at 0) */
static void race_globals_load(RaceState *race)
{
    race->unknown_4c5398 = REPLAY_TICKS_AT_START;
    race->stop_notice_tick = NOTICE_TICK_AT_START;
    race->crash_sound_level = CRASH_SOUND_QUIET;
    race->wheel_counter = 1, race->wheel_toggle = 1;  /* 004c5810, 004c5814 */
    race->strip_best_60 = race->strip_best_100 = race->strip_best_quarter = STRIP_UNSET_AT_START;
    race->engine_sound.unknown_6c = ENGINE_LEVEL_AT_START;
    race->engine_sound.channel = ENGINE_CHANNEL;
    race->engine_sound.pitch = HELD_PITCH_AT_START;  /* 004bcb7c */
    race->engine_sound.rising = 1;                   /* 004bcb88 */
    for (int i = 0; i < HELD_PITCHES; i++) race->sound_pitch[i] = HELD_PITCH_AT_START;
    race->unknown_4c539c = REPLAY_PAUSE_AT_START;
    race->chase_auto = 1;  /* 004b7a28 */
    race->camera_tilt = CAMERA_TILT_AT_START;
    race->default_profile_odds = PROFILE_ODDS_AT_START;
}

void race_controls_from_entries(RaceState *race)
{
    for (int32_t i = 0; i < race->humans && i < RACE_PLAYERS; i++) race->controls[i] = race->entries[i].controls;
}

void race_sim_start_field(RaceSim *sim, const Tri *tri, const char *stem, const CarSpecs *specs,
                          const RaceField *field, int laps, int arcade)
{
    static RaceState before;
    if (!sim->globals_loaded) race_globals_load(&sim->race);
    memcpy(&before, &sim->race, sizeof before);
    memset(sim, 0, sizeof *sim);
    sim->globals_loaded = 1;
    RaceState *race = &sim->race;
    for (size_t i = 0; i < sizeof KEPT / sizeof KEPT[0]; i++)
        memcpy((uint8_t *)race + KEPT[i].at, (const uint8_t *)&before + KEPT[i].at, KEPT[i].size);
    race->tri = tri;
    for (size_t i = 0; stem[i] != '\0' && i + 1 < sizeof race->track_name; i++)
        race->track_name[i] = (char)tolower((unsigned char)stem[i]);
    race->track = race_sim_track_index(stem);
    race->track_variant = race_sim_track_variant(stem);
    race->arcade = arcade;
    race->mirrored = field->mirrored;
    race->port_reverse = field->port_reverse;
    race->track_flags = field->track_flags;
    race->port_difficulty = field->port_difficulty;
    race->port_lunar_gravity = field->port_lunar_gravity;
    race->port_rally_gravel = field->port_rally_gravel;
    race->port_chase_height = field->port_chase_height;
    race->race_type = field->race_type;
    race->racers = field->racers < 1 ? 1 : field->racers > RACE_PLAYERS ? RACE_PLAYERS : field->racers;
    race->cops = field->cops < 0 ? 0 : field->cops;
    race->traffic_setting = field->traffic < 0 ? 0 : difficulty_traffic_count(&race->port_difficulty, field->traffic);
    race->car_count = race->racers + race->cops + race->traffic_setting;  /* 00439c98 */
    if (race->car_count > RACE_CARS) race->car_count = RACE_CARS;     /* port guard: 9 slots */
    race->humans = field->humans > 1 ? (field->humans < race->racers ? field->humans : race->racers) : 1;
    race->phase = 1;
    race->circuit = tri_is_circuit(tri);
    race->segments = (int32_t)tri->ring_segments;  /* 0050420c (0043e4e0): 0x8000 on a stage */
    race->laps_setting = laps;
    race->loop_nodes = EARLY_LOOP_NODES;  /* 0043f994, until 0043fbac sets the real one */
    race_chase_init(race);
    race->chase.kind = 1;
    race->catch_up = !(race->track_flags & NO_MERCY);  /* 0042702c: 004fbac0 = 4, then (004fb7d8 & 4) == 0 */
    race->unknown_4bc6f0 = 1;  /* 004bc6f0 as the exe starts; 00444394 in 00455a5c's setup sets it again */
    race->siren_count = 3;     /* 004c5fb4 and 004c5fbc as the exe starts */
    race->siren_pitch = 0x40;
    race->sounds = (RaceSounds){ race, sim_sound, no_speech, no_tone, sim_push, sim_crash, no_start_line, sim_stop_engine, sim_start_engine, no_buffet, sim_camera, no_jolt, NULL,
                                 NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
    memcpy(race->entries, field->entries, sizeof race->entries);
    /* 004275f8: 0042748c (the grid places), 00428fe0 with 004fb7f4, then
     * 00457870, before 00455a5c. */
    race_grid(race, (int16_t)race->race_type, (int16_t)race->track_variant);
    race_random_seed(race, field->seed);
    race_models(race);
    /* 00455a5c: 004571e8(1), the loading picture (its draws from race_rand) */
    loading_slide(race, field, 1);
    race->controls[0] = (ControlSetup){ 0, 0, 0 };
    sim->count = race->car_count;
    sim->specs[0] = *specs;
    for (int32_t i = 1; i < race->humans; i++)
        if (field->specs[i] != NULL) sim->specs[i] = *field->specs[i];
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
static int32_t sim_wait_timer;  /* 004bb8d8 without the run's: reached only in a network game's playback (0052b312 == 1) */
static void sim_service(void *ctx)
{
    RaceSim *sim = ctx;
    if (sim->service != NULL) sim->service(sim->service_user);
    else sim_wait_timer++;
}
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
    RaceState *race = &((RaceSim *)ctx)->race;
    const RaceSim *sim = ctx;
    const int32_t *timer = sim->service != NULL && sim->timer != NULL ? sim->timer : &sim_wait_timer;
    RaceTickWords w = { race->dropped, 0, 0, 0, sim->wait_ticks, timer, { 0, 0 } };  /* 0052b314: a dropped link game stands */
    race_tick(&o, race, &w);
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
