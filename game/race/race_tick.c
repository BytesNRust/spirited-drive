/* The race's tick (race_tick.h), as 00440ef0 runs it. */
#include "game/race/race_tick.h"

#include "game/race/car_load.h"
#include "game/gfx/x86_int.h"

enum {
    TICK_PHYSICS = 1, TICK_CONTROLS = 4,
    PHASE_RACING = 1, PHASE_HELD = 2, PHASE_OVER = 3, PHASE_STOPPED = 4,
    REST_BEFORE = 0x78, INPUT_REST = 0x7a0,
    FLAG_ACTIVE = 4,
    STOPPED = 0x8000,
    SCREEN_TWO = 4, SCREEN_THREE = 5,
    FAST_TWO = 0x230000, FAST = 0x140000, FASTER = 0x2d0000,
    END_FINISHED = 1, END_ALL_OUT = 0x10,
    VIEW_SHIFT = 3, CHASE_TAKE_KIND = 3, WAIT_EXTRA = 2
};

static Car *car_at(const RaceState *race, int32_t slot)
{
    return race->cars[race_slot(slot)];  /* port guard */
}

/* port guards: the original indexes these tables unchecked */
static PlayerRecord *player_record(RaceState *race, int32_t i)
{
    return &race->players[(uint32_t)i <= RACE_PLAYERS ? i : 0];
}

static void keep_all(RaceState *race)
{
    for (int32_t i = 0; i < race->car_count; i++) car_keep_draw_state(car_at(race, i));
}

/* 00441012..0044111c: with the controls' tick, each active car's
 * controller by road order; then the window sized by the player's speed. */
static void controls(const RaceTickOps *ops, RaceState *race, const RaceTickWords *w)
{
    for (int32_t i = 0; i < race->car_count; i++) {
        Car *car = car_at(race, race->by_node[race_slot(i)]);
        if ((car->flags_51a & FLAG_ACTIVE) && car->controller != 0) ops->controller(ops->ctx, car, car->controller);
    }
    const int32_t speed = car_at(race, race->player)->speed;
    int32_t want;
    if (w->screen_kind == SCREEN_TWO)
        want = speed > FAST_TWO;
    else if (w->screen_kind != SCREEN_THREE)
        return;
    else if (w->high != 0)
        want = speed > FASTER ? 2 : speed > FAST ? 1 : 0;
    else
        want = speed > FAST;
    int32_t first, size;
    ops->window(ops->ctx, &first, &size);
    if (want != size) ops->window_set(ops->ctx, want);
}

/* 00441165..0044125b: 1 when every human has a finish and has stopped; on
 * the way, with two humans or more, the player's view moves on to the
 * first human still racing once the player's own car is done. */
static int humans_done(const RaceTickOps *ops, RaceState *race, RaceTickWords *w)
{
    int done = 1;
    for (int32_t i = 0; i < race->finish_humans; i++) {
        if (player_record(race, i)->finish == 0 || car_at(race, i)->speed > STOPPED) {
            done = 0;
            continue;
        }
        if (race->humans <= 1 || race->phase != PHASE_RACING || i != race->player) continue;
        race->unknown_513e6c = race->phase;
        if (race->unknown_4bc660 != 0) continue;
        for (int32_t j = 0; j < race->humans; j++) {
            if (player_record(race, j)->finish != 0) continue;
            race->viewed = j;
            ops->chase_take(ops->ctx, car_at(race, j));
            race->unknown_4bc660 = 1;
            w->split_views[1] = w->split_views[0] = race->viewed + VIEW_SHIFT;
            ops->engines_start(ops->ctx);
            break;
        }
    }
    return done;
}

/* 0044125b..00441307: the end, after the network game's wait. */
static void race_end(const RaceTickOps *ops, RaceState *race, const RaceTickWords *w)
{
    if (race->playback == 1 && race->unknown_4fbb0c != 0) {
        const int32_t until = x_add(x_add(*w->timer, w->wait_ticks), WAIT_EXTRA);
        while (until > *w->timer) {
            if (w->music_on != 0) ops->music(ops->ctx);
            else ops->service(ops->ctx);
        }
    }
    int all_out = 1;
    for (int32_t i = 0; i < race->finish_humans; i++)
        if (player_record(race, i)->finish > 0) {
            all_out = 0;
            break;
        }
    race->end_request = race->end_state = all_out ? END_ALL_OUT : END_FINISHED;
}

/* 00441362..00441407: the player's progress and the rival's, the other
 * human, the leader (or the second when the player leads), or none. */
static void splits(const RaceTickOps *ops, const RaceState *race)
{
    const int32_t others = x_sub(race->racers, race->humans);
    const int32_t mine = car_at(race, race->player)->progress;
    int32_t rival = 0;
    if (race->humans > 1 || others == 1)
        rival = car_at(race, race->player == 0)->progress;
    else if (others > 1) {
        const int32_t leader = race->by_progress[race_slot(race->car_count - 1)];
        rival = car_at(race, leader != race->player ? leader : race->by_progress[race_slot(race->car_count - 2)])->progress;
    }
    ops->splits_pass(ops->ctx, mine, rival);
}

static void sounds(const RaceTickOps *ops, RaceState *race)
{
    if (race->effects != 0) {
        Car *car = car_at(race, race->player);
        ops->scrape(ops->ctx, car);
        ops->force(ops->ctx, car);
    }
    const int32_t p = (uint32_t)race->player < RACE_PLAYERS ? race->player : 0;  /* port guard */
    if (race->entries[p].sound_volume == 0) return;
    ops->sound_held(ops->ctx);
    ops->sound_follow(ops->ctx);
    ops->sound_472217c(ops->ctx);
    if (race->humans > 1) ops->sound_472348(ops->ctx);
    ops->sound_472560(ops->ctx);
}

void race_tick(const RaceTickOps *ops, RaceState *race, RaceTickWords *w)
{
    if (race->phase != 0) ops->replay(ops->ctx);
    if (race->playback != 0 && w->dropped != 0) return;
    if (race->tick < REST_BEFORE)
        for (int32_t i = 0; i < race->humans && i < RACE_PLAYERS; i++) race->inputs[i] = INPUT_REST;  /* port guard */
    if (race->phase == PHASE_STOPPED ||
        (race->phase == PHASE_HELD && race->unknown_4c5398 == 0 && race->unknown_4c539c <= 0)) {
        keep_all(race);
        return;
    }
    if (race->tick_flags & TICK_CONTROLS) controls(ops, race, w);
    if ((race->tick_flags & TICK_PHYSICS) == 0) return;
    if (race->phase == PHASE_RACING) {
        for (int32_t i = 0; i < race->finish_cars; i++)
            if (car_at(race, i)->flags_51a & FLAG_ACTIVE) ops->finish(ops->ctx, car_at(race, i));
        if (humans_done(ops, race, w)) race_end(ops, race, w);
    }
    for (int32_t i = 0; i < race->car_count; i++) {
        Car *car = car_at(race, race->by_node[race_slot(i)]);
        if ((car->flags_51a & FLAG_ACTIVE) == 0 || car->physics == 0) continue;
        ops->physics(ops->ctx, car, car->physics);
        ops->lateral(ops->ctx, car);
    }
    ops->sort(ops->ctx);
    ops->contacts(ops->ctx);
    ops->sort(ops->ctx);
    splits(ops, race);
    ops->splits_gap(ops->ctx);
    ops->camera_pick(ops->ctx, car_at(race, race->viewed));
    ops->chase_tick(ops->ctx);
    ops->platform(ops->ctx);
    if (race->phase != PHASE_OVER) sounds(ops, race);
    race->drawn_count = 0;
    for (int32_t i = 0; i < race->car_count; i++) {
        Car *car = car_at(race, race->by_node[race_slot(i)]);
        if ((car->flags_51a & FLAG_ACTIVE) == 0 || car->controller == 0) continue;
        race->drawn[race_slot(race->drawn_count)] = car;  /* port guard */
        race->drawn_count++;
        car_keep_draw_state(car);
    }
}
