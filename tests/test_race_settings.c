/* Control Central selection rules (single player), from 00432ff4 and its
 * helpers 00432720/00432974 (race type), 00432c40/00432d48/00432cd4 (track),
 * 00465ec8 (car flags) and the per-panel arrow handling. */
#include "game/frontend/race_settings.h"
#include "game/race/car_registry.h"
#include "test.h"

static const Unlocks NONE = { 0, 0, 0, 0, 0 };
static const Unlocks ALL = { 1, 1, 1, 1, 1 };

static void test_init(void)
{
    RaceSettings s;
    race_settings_init(&s);
    CHECK_EQ_INT(s.race_type, RACE_TIME_TRIAL);
    CHECK_EQ_INT(s.track, 0);
    CHECK_EQ_INT(s.player_car, 0);
    CHECK_EQ_INT(s.opponent, OPPONENT_CLOCK);   /* time trial shows the stopwatch */
    CHECK_EQ_INT(s.laps, 4);                    /* difficulty 0, track 0 */
}

static void test_race_type_cycle(void)
{
    RaceSettings s;
    race_settings_init(&s);
    race_type_step(&s, &NONE, +1);
    CHECK_EQ_INT(s.race_type, RACE_HEAD_TO_HEAD);
    race_type_step(&s, &NONE, +1);
    CHECK_EQ_INT(s.race_type, RACE_SINGLE);
    CHECK_EQ_INT(s.opponent, OPPONENT_FIELD);   /* single race: the whole field */
    race_type_step(&s, &NONE, +1);
    CHECK_EQ_INT(s.race_type, RACE_TOURNAMENT);
    CHECK_EQ_INT(s.opponent, OPPONENT_TOURNAMENT);
    CHECK_EQ_INT(s.difficulty, 1);
    CHECK_EQ_INT(s.laps, 8);
    race_type_step(&s, &NONE, +1);
    CHECK_EQ_INT(s.race_type, RACE_TIME_TRIAL);
    race_type_step(&s, &NONE, -1);
    CHECK_EQ_INT(s.race_type, RACE_TOURNAMENT);
}

static void test_tournament_sets_car_class(void)
{
    RaceSettings s;
    race_settings_init(&s);
    s.track = 6;                                  /* class of the fastest cars */
    s.variant_m = 1;
    s.race_type = RACE_SINGLE;
    race_type_step(&s, &NONE, +1);
    CHECK_EQ_INT(s.race_type, RACE_TOURNAMENT);
    CHECK_EQ_INT(s.player_car, 6);                /* 004baef8[6] */
    CHECK_EQ_INT(s.variant_m, 0);
    CHECK_EQ_INT(s.laps, 6);                      /* difficulty 1, tracks 4..7 */

    player_car_step(&s, &NONE, +1);
    CHECK_EQ_INT(s.player_car, 7);
    player_car_step(&s, &NONE, +1);
    CHECK_EQ_INT(s.player_car, 6);                /* wraps inside the class */
    player_car_step(&s, &NONE, -1);
    CHECK_EQ_INT(s.player_car, 7);
}

static void test_player_car(void)
{
    RaceSettings s;
    race_settings_init(&s);
    player_car_step(&s, &NONE, -1);
    CHECK_EQ_INT(s.player_car, 7);
    player_car_step(&s, &NONE, +1);
    CHECK_EQ_INT(s.player_car, 0);
    s.player_car = 7;
    player_car_step(&s, &ALL, +1);
    CHECK_EQ_INT(s.player_car, CAR_WARRIOR);      /* the secret car after the Diablo */
    player_car_step(&s, &ALL, +1);
    CHECK_EQ_INT(s.player_car, 0);
    player_car_step(&s, &ALL, -1);
    CHECK_EQ_INT(s.player_car, CAR_WARRIOR);
}

static void test_opponent(void)
{
    RaceSettings s;
    race_settings_init(&s);
    race_type_step(&s, &NONE, +1);                /* head to head */
    s.opponent = 0;
    opponent_step(&s, -1);
    CHECK_EQ_INT(s.opponent, 7);
    opponent_step(&s, +1);
    CHECK_EQ_INT(s.opponent, 0);
    race_type_step(&s, &NONE, +1);                /* single race: 0..8 */
    opponent_step(&s, +1);
    CHECK_EQ_INT(s.opponent, 0);                  /* from 8 (field) */
    opponent_step(&s, -1);
    CHECK_EQ_INT(s.opponent, OPPONENT_FIELD);
}

static void test_tracks(void)
{
    RaceSettings s;
    race_settings_init(&s);
    track_step(&s, &NONE, -1);
    CHECK_EQ_INT(s.track, 7);                     /* Lost Vegas locked */
    track_step(&s, &NONE, +1);
    CHECK_EQ_INT(s.track, 0);

    /* With every unlock: base -> m -> r -> next track, and back. */
    track_step(&s, &ALL, +1);
    CHECK(s.track == 0 && s.variant_m == 1 && s.variant_r == 0);
    track_step(&s, &ALL, +1);
    CHECK(s.track == 0 && s.variant_m == 0 && s.variant_r == 1);
    track_step(&s, &ALL, +1);
    CHECK(s.track == 1 && s.variant_m == 0 && s.variant_r == 0);
    track_step(&s, &ALL, -1);
    CHECK(s.track == 0 && s.variant_r == 1);
    track_step(&s, &ALL, -1);
    CHECK(s.track == 0 && s.variant_m == 1 && s.variant_r == 0);
    track_step(&s, &ALL, -1);
    CHECK(s.track == 0 && s.variant_m == 0);
    track_step(&s, &ALL, -1);
    CHECK_EQ_INT(s.track, 9);                     /* the hidden track wraps in */
}

static void test_laps(void)
{
    RaceSettings s;
    race_settings_init(&s);
    s.track = 5;
    track_changed(&s);
    CHECK_EQ_INT(s.laps, 2);
    s.difficulty = 2;
    track_changed(&s);
    CHECK_EQ_INT(s.laps, 12);
    s.track = 8;
    track_changed(&s);
    CHECK_EQ_INT(s.laps, 16);
}

/* Review Focus 8: an added car's flags are those of the car its AI comes from, never a read past the tables of 12 */
static void test_added_car_flags(void)
{
    car_registry_clear();
    CHECK_EQ_INT(car_registry_add(&(CarPiece){ .id = "a", .stem = "A1", .sound_model = 0, .ai_model = 5, .dash_model = -1 }, NULL, 0), 16);
    RaceSettings s, want;
    race_settings_init(&s), race_settings_init(&want);
    want.player_car = 5;
    race_settings_car_flags(&want);
    s.player_car = 16;
    race_settings_car_flags(&s);
    CHECK_EQ_INT(s.car_flag_a, want.car_flag_a);
    CHECK_EQ_INT(s.car_flag_b, want.car_flag_b);
    want.player_car = 0;   /* one not registered, or past every range: car 0's */
    race_settings_car_flags(&want);
    for (int car = 12; car <= 200; car += 4) {
        s.player_car = car;
        race_settings_car_flags(&s);
        if (car == 16) continue;
        CHECK_EQ_INT(s.car_flag_a, want.car_flag_a);
        CHECK_EQ_INT(s.car_flag_b, want.car_flag_b);
    }
    s.player_car = -3;
    race_settings_car_flags(&s);
    CHECK_EQ_INT(s.car_flag_a, want.car_flag_a);
    car_registry_clear();
}

int main(void)
{
    test_init();
    test_race_type_cycle();
    test_tournament_sets_car_class();
    test_player_car();
    test_opponent();
    test_tracks();
    test_laps();
    test_added_car_flags();
    return TEST_REPORT();
}
