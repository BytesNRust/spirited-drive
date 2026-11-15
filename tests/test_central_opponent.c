/* The head to head opponent's arrows (central_select.c): 0..7 as the original steps them, and the Warrior (11)
 * between 7 and 0 with the port's Warrior-as-a-rival extra. dep59 step 3: in Restomod the added cars after the eight, for the player's
 * car and the head to head rival; a single race, a tournament, a link game, Stock and Restored as they were. */
#include "game/frontend/central_run.h"
#include "game/race/car_registry.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

#include "game/platform/platform.h"

/* central_draw.c reaches variant_rule.c, whose rule reads the Mirror and Rally switches through
 * port_extras_load, which reads user/extras.cfg through the platform; the test has no file */
uint8_t *platform_read_user_file(const char *name, size_t *size) { (void)name, (void)size; return NULL; }
bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    (void)name, (void)data, (void)size;
    return false;
}
void platform_free_file(uint8_t *data) { (void)data; }
bool platform_user_file_exists(const char *name)
{
    size_t size = 0;
    uint8_t *data = platform_read_user_file(name, &size);
    if (data != NULL) platform_free_file(data);
    return data != NULL;
}

static int16_t race_type = 1, opponent, net_self, rival, restomod, player_car, warrior, track, warrior_keep = 1;
static int16_t lost;

static void bind(ControlCentralState *st)
{
    st->race_type = &race_type, st->opponent = &opponent, st->net_self = &net_self, st->warrior_rival = &rival, st->restomod = &restomod;
    st->player_car = &player_car, st->warrior = &warrior, st->track = &track, st->warrior_keep = &warrior_keep, st->lost = lost;
}

static int16_t up(int16_t from)
{
    ControlCentralState st = { 0 };
    bind(&st);
    CentralRun r = { 0 };
    r.st = &st;
    opponent = from;
    central_opponent_up(&r, 0);
    return opponent;
}

static int16_t down(int16_t from)
{
    ControlCentralState st = { 0 };
    bind(&st);
    CentralRun r = { 0 };
    r.st = &st;
    opponent = from;
    central_opponent_down(&r, 0);
    return opponent;
}

static void no_flags(void *ctx) { (void)ctx; }

/* the player's car one on (`step` 1) or back from `from` */
static int16_t car(int16_t from, int step)
{
    ControlCentralState st = { 0 };
    bind(&st);
    ControlCentralOps o = { 0 };
    o.car_flags = no_flags;
    CentralRun r = { 0 };
    r.st = &st, r.ops = &o;
    player_car = from;
    if (step > 0) central_car_up(&r);
    else central_car_down(&r);
    return player_car;
}

static void add(const char *id, const char *stem)
{
    CarPiece p;
    memset(&p, 0, sizeof p);
    snprintf(p.id, sizeof p.id, "%s", id);
    snprintf(p.stem, sizeof p.stem, "%s", stem);
    p.dash_model = -1;
    CHECK(car_registry_add(&p, NULL, 0) >= CAR_ADDED_FIRST);
}

static void test_added_cars(void)
{
    car_registry_clear();
    add("a", "A1"), add("b", "B1");   /* 16 and 17 */
    race_type = 1, rival = 0, warrior = 0, lost = 0;
    restomod = 0;   /* Stock and Restored: as the original, the added cars never offered */
    CHECK_EQ_INT(up(7), 0);
    CHECK_EQ_INT(down(0), 7);
    CHECK_EQ_INT(car(7, 1), 0);
    CHECK_EQ_INT(car(0, -1), 7);
    restomod = 1;
    CHECK_EQ_INT(up(7), 16);   /* head to head */
    CHECK_EQ_INT(up(16), 17);
    CHECK_EQ_INT(up(17), 0);
    CHECK_EQ_INT(down(0), 17);
    CHECK_EQ_INT(down(16), 7);
    rival = 1;   /* with the Warrior as a rival: 7, 16, 17, 11, 0 */
    CHECK_EQ_INT(up(7), 16);
    CHECK_EQ_INT(up(17), 11);
    CHECK_EQ_INT(up(11), 0);
    CHECK_EQ_INT(down(0), 11);
    CHECK_EQ_INT(down(11), 17);
    rival = 0;
    CHECK_EQ_INT(car(7, 1), 16);   /* the player's car */
    CHECK_EQ_INT(car(17, 1), 0);
    CHECK_EQ_INT(car(0, -1), 17);
    warrior = 1;
    CHECK_EQ_INT(car(17, 1), 11);
    CHECK_EQ_INT(car(11, 1), 0);
    CHECK_EQ_INT(car(11, -1), 17);
    CHECK_EQ_INT(car(0, -1), 11);
    warrior = 0;
    race_type = 0;   /* a time trial: the player's car only, no rival */
    CHECK_EQ_INT(car(7, 1), 16);
    race_type = 2;   /* a single race: the opponent keeps its 0..8 (the field), the player's car its list */
    CHECK_EQ_INT(up(8), 0);
    CHECK_EQ_INT(down(0), 8);
    race_type = 3, track = 0;   /* a tournament: its class */
    CHECK_EQ_INT(car(2, 1), 0);
    CHECK_EQ_INT(car(0, -1), 2);
    race_type = 1, lost = 1, net_self = 1;   /* a link game: the original's */
    CHECK_EQ_INT(car(7, 1), 0);
    CHECK_EQ_INT(car(0, -1), 7);
    lost = 0, net_self = 0;
    /* the screen's start: an added car no longer registered goes back to car 0, one that is stays */
    ControlCentralState st = { 0 };
    bind(&st);
    player_car = 17;
    central_car_start(&st);
    CHECK_EQ_INT(player_car, 17);
    player_car = 40;
    central_car_start(&st);
    CHECK_EQ_INT(player_car, 0);
    restomod = 0, player_car = 17;   /* nor in Stock and Restored */
    central_car_start(&st);
    CHECK_EQ_INT(player_car, 0);
    car_registry_clear();
    race_type = 1;
}

/* The CD's files are not read by this test (the tables start as the built-in defaults). */
uint8_t *platform_read_file(const char *path, size_t *size) { (void)path; (void)size; return NULL; }

int main(void)
{
    rival = 0;
    CHECK_EQ_INT(up(6), 7);
    CHECK_EQ_INT(up(7), 0);
    CHECK_EQ_INT(down(0), 7);
    CHECK_EQ_INT(up(11), 0);      /* the original's own way back from 11 */
    CHECK_EQ_INT(down(11), 7);
    rival = 1;
    CHECK_EQ_INT(up(7), 11);
    CHECK_EQ_INT(up(11), 0);
    CHECK_EQ_INT(down(0), 11);
    CHECK_EQ_INT(down(11), 7);
    CHECK_EQ_INT(up(3), 4);
    /* a single race (type 2) keeps its 0..8 */
    race_type = 2;
    CHECK_EQ_INT(up(8), 0);
    CHECK_EQ_INT(down(0), 8);
    test_added_cars();
    return TEST_REPORT();
}
