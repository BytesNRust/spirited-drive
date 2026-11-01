/* port (extras.cfg's difficulty, difficulty.h): Original is the exe's, bit for bit; the Easy and Hard presets; the replay's stamp and the
 * forcing (a link race and a replay without a stamp run Original, a replay plays the difficulty it was recorded with). */
#include "game/race/ai_setup.h"
#include "game/race/difficulty.h"
#include "game/race/race_replay_image.h"
#include "test.h"

#include <string.h>

enum { PARAMS = 107, ROW_FIRST = 23, ROW_LAST = 64, LEAD = 6, STOPS = 7, FACTOR = 14 };

static RaceDifficulty all(int32_t level)
{
    const RaceDifficulty d = { level, level, level };
    return d;
}

static void test_original(void)
{
    int32_t exe[PARAMS], p[PARAMS];
    ai_default_params(exe);
    memcpy(p, exe, sizeof p);
    const RaceDifficulty o = all(DIFFICULTY_ORIGINAL);
    difficulty_apply_params(&o, p);
    CHECK(memcmp(exe, p, sizeof p) == 0);   /* no word touched */
    CHECK(difficulty_is_original(&o));
    CHECK_EQ_INT(difficulty_traffic_count(&o, 6), 6);
    for (int k = 0; k < 4; k++) CHECK_EQ_INT(difficulty_police_odds(&o, k, 32 * (k + 1)), 32 * (k + 1));
    CHECK_EQ_INT(difficulty_stops_to_end(&o, 1), 1);
    CHECK(!difficulty_rivals_flat(&o));
    CHECK_EQ_INT(difficulty_bend_scale(&o, 0x12345), 0x12345);
    const RaceDifficulty zero = { 0, 0, 0 };
    CHECK(difficulty_is_original(&zero));
    const RaceDifficulty junk = { 7, -1, 9 };   /* a value out of range reads as Original */
    CHECK(difficulty_is_original(&junk));
    CHECK_EQ_INT(difficulty_stamp(&o), 0);
}

static void test_easy(void)
{
    int32_t exe[PARAMS], p[PARAMS];
    ai_default_params(exe);
    memcpy(p, exe, sizeof p);
    const RaceDifficulty e = all(DIFFICULTY_EASY);
    difficulty_apply_params(&e, p);
    for (int i = ROW_FIRST; i <= ROW_LAST; i++) {
        CHECK(p[i] <= 0x14ccc);                          /* at most 1.3 */
        CHECK(p[i] <= exe[i]);                           /* never faster than the exe's */
    }
    CHECK(p[ROW_FIRST + 10] < exe[ROW_FIRST + 10]);       /* level with the first: slower */
    CHECK_EQ_INT(p[ROW_FIRST + 10], (int32_t)((int64_t)exe[ROW_FIRST + 10] * 85 / 100));
    CHECK_EQ_INT(p[LEAD], exe[LEAD] * 3 / 4);
    CHECK_EQ_INT(p[STOPS], 800);
    CHECK(p[FACTOR] >= exe[FACTOR] && p[FACTOR + 2] > exe[FACTOR + 2]);   /* slower traffic is harder: Easy's is faster */
    CHECK_EQ_INT(difficulty_traffic_count(&e, 6), 4);
    CHECK_EQ_INT(difficulty_traffic_count(&e, 2), 2);
    CHECK_EQ_INT(difficulty_police_odds(&e, 3, 128), 64);
    CHECK_EQ_INT(difficulty_stops_to_end(&e, 1), 2);
    CHECK(!difficulty_rivals_flat(&e));
    CHECK(!difficulty_is_original(&e));
}

static void test_hard(void)
{
    int32_t exe[PARAMS], p[PARAMS];
    ai_default_params(exe);
    memcpy(p, exe, sizeof p);
    const RaceDifficulty h = all(DIFFICULTY_HARD);
    difficulty_apply_params(&h, p);
    for (int i = ROW_FIRST; i <= ROW_LAST; i++) CHECK_EQ_INT(p[i], 0x10000);   /* no catch-up band */
    CHECK_EQ_INT(p[LEAD], exe[LEAD]);
    CHECK_EQ_INT(p[STOPS], 200);
    CHECK(p[FACTOR] < exe[FACTOR] && p[FACTOR + 2] < exe[FACTOR + 2]);   /* more closing speed */
    CHECK_EQ_INT(difficulty_traffic_count(&h, 6), 6);   /* the count stays */
    CHECK_EQ_INT(difficulty_police_odds(&h, 0, 32), 48);
    CHECK_EQ_INT(difficulty_police_odds(&h, 3, 128), 128);
    CHECK_EQ_INT(difficulty_stops_to_end(&h, 1), 1);
    CHECK(difficulty_rivals_flat(&h));
    CHECK_EQ_INT(difficulty_bend_scale(&h, 0x10000), 0x14ccc);   /* x1.3 */
    /* the axes are independent: Hard rivals with Original police and traffic touch only the rivals' words */
    const RaceDifficulty mixed = { DIFFICULTY_HARD, DIFFICULTY_ORIGINAL, DIFFICULTY_ORIGINAL };
    memcpy(p, exe, sizeof p);
    difficulty_apply_params(&mixed, p);
    CHECK_EQ_INT(p[STOPS], exe[STOPS]);
    CHECK_EQ_INT(p[FACTOR], exe[FACTOR]);
    CHECK_EQ_INT(p[ROW_FIRST], 0x10000);
    CHECK_EQ_INT(difficulty_police_odds(&mixed, 2, 96), 96);
}

static void test_forcing(void)
{
    const RaceDifficulty hard = all(DIFFICULTY_HARD), orig = all(DIFFICULTY_ORIGINAL);
    RaceDifficulty d = difficulty_resolve(0, NULL, 0);   /* a race: extras.cfg's level */
    CHECK(d.rivals == DIFFICULTY_EASY && d.police == DIFFICULTY_EASY && d.traffic == DIFFICULTY_EASY);
    d = difficulty_resolve(1, NULL, 0);
    CHECK(difficulty_is_original(&d));
    d = difficulty_resolve(2, NULL, 0);
    CHECK(d.rivals == DIFFICULTY_HARD && d.traffic == DIFFICULTY_HARD);
    d = difficulty_resolve(9, NULL, 0);                  /* out of range: Original */
    CHECK(difficulty_is_original(&d));
    d = difficulty_resolve(2, NULL, 1);                  /* a link race: Original whatever extras.cfg says */
    CHECK(difficulty_is_original(&d));
    d = difficulty_resolve(0, &hard, 0);                 /* a replay plays its own, not extras.cfg's */
    CHECK(d.rivals == DIFFICULTY_HARD);
    d = difficulty_resolve(2, &orig, 0);                 /* a replay without a stamp (the CD's demos): Original */
    CHECK(difficulty_is_original(&d));
    d = difficulty_resolve(0, &hard, 1);
    CHECK(difficulty_is_original(&d));
}

static void test_stamp(void)
{
    for (int32_t r = 0; r < 3; r++)
        for (int32_t p = 0; p < 3; p++)
            for (int32_t t = 0; t < 3; t++) {
                const RaceDifficulty d = { r, p, t };
                const unsigned s = difficulty_stamp(&d);
                CHECK_EQ_INT(s & 3, 0);      /* EXPLODE and NITRO's bits are left alone */
                CHECK(s <= 0xff);
                const RaceDifficulty back = difficulty_unstamp(s);
                CHECK(back.rivals == r && back.police == p && back.traffic == t);
            }
}

/* the replay's setup: an Original race writes what it always wrote (the port's cheat bytes 0 without a cheat), another stamps and reads back */
static void test_replay_setup(void)
{
    static RaceState race;
    static uint8_t setup[REPLAY_SETUP_SIZE];
    memset(&race, 0, sizeof race);
    strcpy(race.track_name, "TR1");
    race.race_type = 1, race.racers = 2, race.humans = 1, race.cops = 1, race.traffic_setting = 6, race.car_count = 9;
    race.laps_setting = 2, race.player = 0;
    race_replay_image_setup(&race, 0x1234, setup);
    CHECK(setup[0x3c] == 0 && setup[0x3d] == 0);   /* Original, no cheat: the bytes of the exe's setup */
    ReplayRace r;
    CHECK(race_replay_image_race(setup, &r));
    CHECK(difficulty_is_original(&r.field.port_difficulty));

    race.port_difficulty = all(DIFFICULTY_HARD);
    race_replay_image_setup(&race, 0x1234, setup);
    CHECK(setup[0x3d] == 0xa5 && (setup[0x3c] & 3) == 0 && setup[0x3c] != 0);
    CHECK(race_replay_image_race(setup, &r));
    CHECK(r.field.port_difficulty.rivals == DIFFICULTY_HARD && r.field.port_difficulty.police == DIFFICULTY_HARD &&
          r.field.port_difficulty.traffic == DIFFICULTY_HARD);
    CHECK(!r.explode && !r.nitro);

    race.port_difficulty.rivals = DIFFICULTY_EASY, race.port_difficulty.traffic = DIFFICULTY_ORIGINAL, race.unknown_4c5cd4 = 1;
    race_replay_image_setup(&race, 0x1234, setup);   /* with a cheat besides */
    CHECK(race_replay_image_race(setup, &r));
    CHECK(r.explode && r.field.port_difficulty.rivals == DIFFICULTY_EASY && r.field.port_difficulty.police == DIFFICULTY_HARD &&
          r.field.port_difficulty.traffic == DIFFICULTY_ORIGINAL);
}

int main(void)
{
    const RaceDifficulty e1 = difficulty_from_extras(0), e2 = difficulty_from_extras(1), e3 = difficulty_from_extras(2);
    CHECK(e1.rivals == DIFFICULTY_EASY && difficulty_is_original(&e2) && e3.police == DIFFICULTY_HARD);
    test_original();
    test_easy();
    test_hard();
    test_forcing();
    test_stamp();
    test_replay_setup();
    return TEST_REPORT();
}
