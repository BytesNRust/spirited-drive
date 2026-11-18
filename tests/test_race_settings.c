/* Control Central selection rules (single player), from 00432ff4 and its
 * helpers 00432720/00432974 (race type), 00432c40/00432d48/00432cd4 (track),
 * 00465ec8 (car flags) and the per-panel arrow handling. */
#include "game/frontend/race_settings.h"
#include "game/frontend/config_dat.h"
#include "game/frontend/tournament.h"
#include "game/race/car_registry.h"
#include "game/frontend/tournament_car.h"
#include "game/sys/track_registry.h"

#include <stdio.h>
#include <string.h>
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

/* Review Focus 8; 3b Decision 5: an added car's flags are CAR_FLAGS_ADDED_A/B (the originals' majority), whatever it is made from, never a
 * read past the tables of 12 */
static void test_added_car_flags(void)
{
    car_registry_clear();
    CHECK_EQ_INT(car_registry_add(&(CarPiece){ .id = "a", .stem = "A1", .sound_model = 5, .ai = { CAR_AI_PACE_MIN, CAR_AI_COAST_DEFAULT }, .dash_model = -1 }, NULL, 0), 16);
    RaceSettings s, want;
    race_settings_init(&s), race_settings_init(&want);
    s.player_car = 16;
    race_settings_car_flags(&s);
    CHECK_EQ_INT(s.car_flag_a, CAR_FLAGS_ADDED_A ? 0 : 2);   /* A = 1, B = 0: flag a 0, flag b 2 (the Viper, car 5, would give 2 and 2) */
    CHECK_EQ_INT(s.car_flag_b, (CAR_FLAGS_ADDED_B == 0) + 1);
    CHECK_EQ_INT(CAR_FLAGS_ADDED_A, 1);
    CHECK_EQ_INT(CAR_FLAGS_ADDED_B, 0);
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

static int never_asked(int german, void *ctx) { (void)german, (void)ctx; return 0; }

/* dep59 step 4, T7: an edition that closes the tournament (FrontendSession.no_tournament): Control Central's race type arrows pass it both
 * ways, and leave nothing of it behind (the car, the difficulty); without it, as the exe */
static void test_the_arrows_pass_a_closed_tournament(void)
{
    enum { LEFT = 0x4b00, RIGHT = 0x4d00 };
    static uint8_t state[GAME_STATE_SIZE];
    static ConfigDat config;
    RaceSettings s;
    race_settings_init(&s);
    Unlocks u = { 0, 0, 0, 0, 0 };
    FrontendSession f = { 0 };
    f.no_tournament = 1;
    s.race_type = RACE_SINGLE, s.player_car = 5, s.difficulty = 2;
    race_settings_type_step(&s, &u, &f, state, &config, RIGHT, never_asked, NULL);
    CHECK_EQ_INT(s.race_type, RACE_TIME_TRIAL);
    CHECK_EQ_INT(s.player_car, 5);
    CHECK_EQ_INT(s.difficulty, 2);
    race_settings_type_step(&s, &u, &f, state, &config, LEFT, never_asked, NULL);
    CHECK_EQ_INT(s.race_type, RACE_SINGLE);
    f.no_tournament = 0;
    race_settings_type_step(&s, &u, &f, state, &config, RIGHT, never_asked, NULL);
    CHECK_EQ_INT(s.race_type, RACE_TOURNAMENT);
}

/* dep59 step 5, T7a2: the tournament's car and laps read tables of ten: a track number past them (an added track's 16..79, -1) reads none, and
 * a tournament never keeps an added track: entering one puts the first track, as the hidden one does */
static void test_a_tournament_reads_no_table_past_ten(void)
{
    static const int TRACKS[] = { -1, 9, 10, 16, 79 };
    for (size_t k = 0; k < sizeof TRACKS / sizeof TRACKS[0]; k++) {
        RaceSettings s;
        race_settings_init(&s);
        s.race_type = RACE_TOURNAMENT;
        s.track = TRACKS[k];
        track_changed(&s);
        CHECK_EQ_INT(s.player_car, TRACKS[k] == 9 ? 3 : 0);   /* the table's tenth word (the hidden track's), else the first track's */
    }
    static const short TABLE[3] = { 5, 6, 7 };   /* the macro the Control Central and Options sites share */
    CHECK(TOURNAMENT_CAR_OF(TABLE, 0) == 5 && TOURNAMENT_CAR_OF(TABLE, 2) == 7 && TOURNAMENT_CAR_OF(TABLE, 3) == 5 && TOURNAMENT_CAR_OF(TABLE, -1) == 5 && TOURNAMENT_CAR_OF(TABLE, 79) == 5);
    TrackPiece piece;
    memset(&piece, 0, sizeof piece);
    snprintf(piece.id, sizeof piece.id, "dune"), snprintf(piece.name, sizeof piece.name, "dune"), snprintf(piece.stem, sizeof piece.stem, "ZZA");
    piece.spec.kind = TRACK_KIND_STAGE, piece.spec.segments = 1, piece.spec.times = TRACK_TIME_DAY, piece.fingerprint = 7;
    track_registry_clear();
    CHECK(track_registry_add(&piece, NULL, 0) == 16);
    RaceSettings s;
    race_settings_init(&s);
    s.track = 16, s.race_type = RACE_SINGLE;
    race_type_step(&s, &NONE, +1);
    CHECK_EQ_INT(s.race_type, RACE_TOURNAMENT);
    CHECK_EQ_INT(s.track, 0);                     /* the first track, as for the hidden one */
    CHECK_EQ_INT(s.player_car, 0);
    track_registry_clear();
}

int main(void)
{
    test_init();
    test_the_arrows_pass_a_closed_tournament();
    test_race_type_cycle();
    test_tournament_sets_car_class();
    test_player_car();
    test_opponent();
    test_tracks();
    test_laps();
    test_added_car_flags();
    test_a_tournament_reads_no_table_past_ten();
    return TEST_REPORT();
}
